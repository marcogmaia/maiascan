import os
import sys
import subprocess
import argparse
import shutil
import time
import platform
import re
import json
import ctypes
import locale
from pathlib import Path
from datetime import datetime
from dataclasses import dataclass
from typing import List, Optional, Dict, Tuple, Pattern

# --- Configuration ---
DEFAULT_PRESET = "windows-release" if os.name == "nt" else "linux-debug"

# Dynamic Source Filter: Matches files inside the CWD/src folder
# We use re.escape to handle Windows backslashes (C:\...) correctly
try:
    _src_path = os.path.join(os.getcwd(), "src")
    DEFAULT_SOURCE_FILTER = re.escape(str(_src_path)) + ".*"
except Exception:
    DEFAULT_SOURCE_FILTER = ".*"

# ANSI Colors
COLOR_RESET = "\033[0m"
COLOR_RED = "\033[91m"
COLOR_YELLOW = "\033[93m"
COLOR_CYAN = "\033[96m"
COLOR_GREEN = "\033[92m"
COLOR_BOLD = "\033[1m"
COLOR_DIM = "\033[2m"


def enable_vt_processing():
    """Enables virtual terminal processing for ANSI colors on Windows."""
    if os.name == "nt":
        try:
            kernel32 = ctypes.windll.kernel32
            handle = kernel32.GetStdHandle(-11)  # STD_OUTPUT_HANDLE
            mode = ctypes.c_ulong()
            kernel32.GetConsoleMode(handle, ctypes.byref(mode))
            # ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004
            kernel32.SetConsoleMode(handle, mode.value | 0x0004)
        except Exception:
            pass


@dataclass
class LintIssue:
    file: str
    line: str
    col: str
    severity: str
    message: str
    check: str

    def __str__(self):
        color = COLOR_RED if self.severity == "error" else COLOR_YELLOW
        # Normalize path for display (show relative if possible)
        try:
            display_file = Path(self.file).relative_to(os.getcwd())
        except ValueError:
            display_file = Path(self.file).name

        return (
            f"{COLOR_BOLD}{display_file}:{self.line}:{self.col}{COLOR_RESET}: "
            f"{color}{self.severity}{COLOR_RESET}: {self.message} "
            f"[{COLOR_CYAN}{self.check}{COLOR_RESET}]"
        )


class OutputParser:
    """Parses clang-tidy output streams."""

    def __init__(self):
        # Regex handles C:\path (Windows) and /path (Linux)
        self.pattern: Pattern = re.compile(
            r"^((?:[a-zA-Z]:)?[^:]+):(\d+):(\d+): (error|warning|note): (.+) \[(.+)\]$"
        )
        self.ansi_escape: Pattern = re.compile(r"\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])")
        self.issues: List[LintIssue] = []

    def strip_ansi(self, text: str) -> str:
        return self.ansi_escape.sub("", text)

    def parse_line(self, line: str) -> Optional[LintIssue]:
        clean_line = self.strip_ansi(line).strip()
        match = self.pattern.match(clean_line)
        if match:
            issue = LintIssue(
                file=match.group(1),
                line=match.group(2),
                col=match.group(3),
                severity=match.group(4),
                message=match.group(5),
                check=match.group(6),
            )
            if issue.severity in ["error", "warning"]:
                self.issues.append(issue)
            return issue
        return None


class Toolchain:
    """Handles locating external build tools and setting up the environment."""

    def __init__(self, env: Dict[str, str]):
        self.env = env
        self.tools: Dict[str, str] = {}

    def probe(self):
        """Checks versions of critical tools."""
        self.tools["cmake"] = self._get_version(["cmake", "--version"])
        self.tools["clang-tidy"] = self._get_version(["clang-tidy", "--version"])

        if os.name == "nt":
            self.tools["msvc"] = self._get_version(["cl"], stderr=True)
        else:
            self.tools["clang"] = self._get_version(["clang", "--version"])

    def _get_version(self, cmd: List[str], stderr: bool = False) -> str:
        try:
            res = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                env=self.env,
                shell=(os.name == "nt"),
            )
            output = res.stderr if stderr else res.stdout
            if not output and not stderr:
                output = res.stderr
            return output.splitlines()[0] if output else "Unknown"
        except (FileNotFoundError, PermissionError):
            return "Not found"

    def find_run_clang_tidy(self) -> Optional[Path]:
        """Locates the run-clang-tidy python script or executable."""
        path_val = self.env.get("PATH", "")
        search_names = ["run-clang-tidy", "run-clang-tidy.py", "run-clang-tidy-14.py"]

        # 1. Check current toolchain environment (PATH)
        for p in path_val.split(os.pathsep):
            for name in search_names:
                candidate = Path(p) / name
                if candidate.exists():
                    return candidate

        # 2. Check relative to clang-tidy binary
        clang_tidy_path = shutil.which("clang-tidy", path=path_val)
        if clang_tidy_path:
            ct_bin = Path(clang_tidy_path).parent
            search_dirs = [
                ct_bin,
                ct_bin.parent / "share" / "clang",
                ct_bin.parent / "bin",
            ]
            for d in search_dirs:
                for name in search_names:
                    candidate = d / name
                    if candidate.exists():
                        return candidate

        # 3. Windows Standard paths
        if os.name == "nt":
            prog_files = [
                os.environ.get("ProgramFiles", "C:/Program Files"),
                os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)"),
            ]
            for pf in prog_files:
                base = Path(pf) / "LLVM"
                if base.exists():
                    for sub in ["bin", "share/clang"]:
                        for name in search_names:
                            candidate = base / sub / name
                            if candidate.exists():
                                return candidate
        return None

    def find_clang_apply_replacements(self) -> Optional[Path]:
        path_val = self.env.get("PATH", "")
        from_path = shutil.which("clang-apply-replacements", path=path_val)
        if from_path:
            return Path(from_path)

        clang_tidy_path = shutil.which("clang-tidy", path=path_val)
        if clang_tidy_path:
            candidate = Path(clang_tidy_path).parent / "clang-apply-replacements"
            if os.name == "nt":
                candidate = candidate.with_suffix(".exe")
            if candidate.exists():
                return candidate
        return None


class ProjectContext:
    def __init__(self, preset: str, root_dir: Path, toolchain: Toolchain):
        self.preset = preset
        self.root_dir = root_dir
        self.toolchain = toolchain
        self.start_time = time.time()
        self.cpu_count = os.cpu_count() or 1

    def print_header(self):
        print(f"{COLOR_BOLD}{'=' * 60}")
        print(f"LINT PIPELINE - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"{'=' * 60}{COLOR_RESET}")
        print(f"OS:       {platform.system()} {platform.release()}")
        print(f"Preset:   {self.preset}")
        print(f"Root:     {self.root_dir}")
        print(f"Cores:    {self.cpu_count}")
        print(f"{'-' * 60}")
        print("Tools:")
        for k, v in self.toolchain.tools.items():
            print(f" • {k:<12} : {v}")
        print(f"{'=' * 60}{COLOR_RESET}\n")

    def print_summary(self, success: bool, issue_count: int):
        duration = time.time() - self.start_time
        status = (
            f"{COLOR_GREEN}✅ SUCCESS{COLOR_RESET}"
            if success
            else f"{COLOR_RED}❌ FAILED{COLOR_RESET}"
        )

        print("\n" + "=" * 60)
        print(f"{COLOR_BOLD}LINT SUMMARY{COLOR_RESET}")
        print(f"Result:   {status}")
        print(f"Issues:   {issue_count}")
        print(f"Duration: {duration:.2f}s")
        print("=" * 60 + "\n")


def get_toolset_from_preset(preset_name: str, root_dir: Path) -> Optional[str]:
    """
    Parses CMakePresets.json to find the 'toolset' version.
    Now supports:
      1. Preset Inheritance (recursively checks 'inherits').
      2. Complex strings (e.g., "version=14.44" with spaces).
      3. Dictionary-style toolset definitions.
    """
    # 1. Load ALL presets into a lookup map
    presets_map = {}
    preset_files = ["CMakePresets.json", "CMakeUserPresets.json"]

    for filename in preset_files:
        preset_path = root_dir / filename
        if not preset_path.exists():
            continue
        try:
            with open(preset_path, "r", encoding="utf-8") as f:
                data = json.load(f)
                # We map names to preset objects to allow random access for inheritance
                for p in data.get("configurePresets", []):
                    presets_map[p["name"]] = p
        except Exception:
            continue

    # 2. Recursive resolver
    def find_version_recursive(current_name: str, visited: set) -> Optional[str]:
        if current_name not in presets_map or current_name in visited:
            return None
        visited.add(current_name)

        preset = presets_map[current_name]

        # A. Check if THIS preset has the toolset
        toolset = preset.get("toolset")
        if toolset:
            # Extract the raw string value
            ts_value = toolset if isinstance(toolset, str) else toolset.get("value", "")

            # Strategy 1: Look for "version=14.xx" (flexible spaces)
            match = re.search(r"version\s*=\s*([0-9.]+)", ts_value, re.IGNORECASE)
            if match:
                return match.group(1)

            # Strategy 2: If the whole string looks like a version "14.40"
            if re.match(r"^[0-9.]+$", ts_value.strip()):
                return ts_value.strip()

        # B. Check inherited presets (Depth-First)
        inherits = preset.get("inherits", [])
        if isinstance(inherits, str):
            inherits = [inherits]

        for parent_name in inherits:
            found = find_version_recursive(parent_name, visited)
            if found:
                return found

        return None

    # Start the search
    return find_version_recursive(preset_name, set())


def get_msvc_environment(toolset_version: Optional[str] = None) -> Dict[str, str]:
    """
    Robustly finds MSVC and loads environment variables via vcvars64.bat.
    """
    if os.name != "nt":
        return os.environ.copy()

    # If VC is already loaded, skip unless a specific version is forced
    if "VCINSTALLDIR" in os.environ and not toolset_version:
        return os.environ.copy()

    # 1. Locate vswhere
    vswhere_path = None
    from_path = shutil.which("vswhere")
    if from_path:
        vswhere_path = Path(from_path)
    else:
        default_path = (
            Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)"))
            / "Microsoft Visual Studio/Installer/vswhere.exe"
        )
        if default_path.exists():
            vswhere_path = default_path

    if not vswhere_path:
        return os.environ.copy()

    # 2. Get Install Path
    try:
        install_path = subprocess.check_output(
            [
                str(vswhere_path),
                "-latest",
                "-products",
                "*",
                "-requires",
                "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                "-property",
                "installationPath",
            ],
            text=True,
        ).strip()
    except subprocess.CalledProcessError:
        return os.environ.copy()

    if not install_path:
        return os.environ.copy()

    vcvars_path = Path(install_path) / "VC/Auxiliary/Build/vcvars64.bat"

    if not vcvars_path.exists():
        print(
            f"{COLOR_YELLOW}[WARN] vcvars64.bat NOT found at: {vcvars_path}{COLOR_RESET}"
        )
        return os.environ.copy()

    # 3. Execution Magic
    separator = "[MAGIC_ENV_START]"
    vcvars_arg = f"-vcvars_ver={toolset_version}" if toolset_version else ""

    # FIX: Construct raw string and use shell=True to avoid Python escaping quotes incorrectly
    cmd_str = f'"{str(vcvars_path)}" {vcvars_arg} && echo {separator} && set'

    try:
        sys_encoding = locale.getpreferredencoding()

        result = subprocess.run(
            cmd_str,
            capture_output=True,
            text=True,
            encoding=sys_encoding,
            errors="replace",
            shell=True,
        )

        if result.returncode != 0:
            print(f"{COLOR_RED}[ERR] MSVC setup script failed!{COLOR_RESET}")
            print(f"{COLOR_DIM}Path tried: {vcvars_path}{COLOR_RESET}")
            print(f"{COLOR_RED}Output:\n{result.stderr or result.stdout}{COLOR_RESET}")
            sys.exit(1)

        output = result.stdout
        if separator not in output:
            return os.environ.copy()

        _, env_block = output.split(separator, 1)

        new_env = os.environ.copy()
        for line in env_block.splitlines():
            line = line.strip()
            if "=" in line:
                key, value = line.split("=", 1)
                new_env[key] = value

        return new_env

    except Exception as e:
        print(f"{COLOR_YELLOW}[WARN] Failed to load MSVC environment: {e}{COLOR_RESET}")
        return os.environ.copy()


def run_lint_streaming(
    cmd: List[str], env: Dict[str, str], verbose: bool = False
) -> Tuple[int, List[LintIssue]]:
    """Runs clang-tidy and parses output in real-time."""
    if verbose:
        print(f"{COLOR_DIM}[CMD] {' '.join(cmd)}{COLOR_RESET}")

    parser = OutputParser()

    process = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=env,
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,
    )

    if process.stdout:
        for line in process.stdout:
            issue = parser.parse_line(line)
            if issue:
                print(str(issue))
            else:
                stripped = line.strip()
                if not stripped:
                    continue
                if "warnings generated" in stripped:
                    continue
                if "Suppressed" in stripped and "non-user code" in stripped:
                    continue

                if verbose or "error:" in stripped or "Enabled checks:" in stripped:
                    print(f"{COLOR_DIM}{stripped}{COLOR_RESET}")

    process.wait()
    return process.returncode, parser.issues


def print_issue_summary_table(issues: List[LintIssue]):
    if not issues:
        return

    print("\n" + "-" * 60)
    print(f"{COLOR_BOLD}ISSUE BREAKDOWN{COLOR_RESET}")
    print("-" * 60)

    summary = {}
    for issue in issues:
        summary[issue.check] = summary.get(issue.check, 0) + 1

    sorted_summary = sorted(summary.items(), key=lambda x: x[1], reverse=True)

    print(f"{'Check Name':<45} | {'Count':<10}")
    print("-" * 60)
    for check, count in sorted_summary:
        print(f"{check:<45} | {count:<10}")
    print("-" * 60)


def confirm_action(message: str) -> bool:
    while True:
        response = (
            input(f"{COLOR_YELLOW}{message} [y/N]: {COLOR_RESET}").lower().strip()
        )
        if response in ["y", "yes"]:
            return True
        if response in ["", "n", "no"]:
            return False


def main():
    enable_vt_processing()

    parser = argparse.ArgumentParser(description="Lint Script")
    parser.add_argument(
        "--preset",
        default=DEFAULT_PRESET,
        help=f"CMake preset (default: {DEFAULT_PRESET})",
    )
    parser.add_argument(
        "--root", default=os.getcwd(), help="Project root directory (default: CWD)"
    )
    parser.add_argument(
        "--fix", action="store_true", help="Apply suggested fixes to enabled checks"
    )
    parser.add_argument(
        "--force", action="store_true", help="Skip confirmation prompt for --fix"
    )
    parser.add_argument(
        "--filter",
        default=DEFAULT_SOURCE_FILTER,
        help=f"Regex for source files to check (default: {DEFAULT_SOURCE_FILTER})",
    )
    parser.add_argument(
        "--msvc-toolset",
        default=None,
        help="Specific MSVC toolset version (e.g. 14.40)",
    )
    parser.add_argument(
        "--verbose", action="store_true", help="Print full commands and debug info"
    )
    args = parser.parse_args()

    root_dir = Path(args.root).resolve()

    # 1. Detect MSVC Toolset from CMake Presets
    if os.name == "nt" and args.msvc_toolset is None:
        detected_toolset = get_toolset_from_preset(args.preset, root_dir)
        if detected_toolset:
            if args.verbose:
                print(
                    f"[LINT] Detected MSVC toolset {detected_toolset} from preset '{args.preset}'"
                )
            args.msvc_toolset = detected_toolset

    # 2. Setup Environment
    print(
        f"[LINT] Probing environment (Preset: {args.preset}, Toolset: {args.msvc_toolset or 'Default'})..."
    )
    env = get_msvc_environment(toolset_version=args.msvc_toolset)
    toolchain = Toolchain(env)
    toolchain.probe()

    build_dir = root_dir / "out" / "build" / args.preset
    compile_commands = build_dir / "compile_commands.json"

    ctx = ProjectContext(args.preset, root_dir, toolchain)
    ctx.print_header()

    # 3. Ensure compile_commands.json exists
    if not compile_commands.exists():
        print(f"[LINT] compile_commands.json not found in {build_dir}. Configuring...")
        build_dir.parent.mkdir(parents=True, exist_ok=True)

        res = subprocess.run(
            ["cmake", "--preset", args.preset],
            env=env,
            cwd=str(root_dir),
            shell=(os.name == "nt"),
        )
        if res.returncode != 0:
            print(f"{COLOR_RED}[ERR] CMake configuration failed.{COLOR_RESET}")
            sys.exit(res.returncode)

    if not compile_commands.exists():
        print(
            f"{COLOR_RED}[ERR] Failed to generate compile_commands.json at {compile_commands}{COLOR_RESET}"
        )
        sys.exit(1)

    # 4. Find Tools
    run_ct = toolchain.find_run_clang_tidy()
    if not run_ct:
        print(
            f"{COLOR_RED}[ERR] run-clang-tidy not found. Please ensure LLVM is installed.{COLOR_RESET}"
        )
        sys.exit(1)

    print(f"[LINT] Linter script: {run_ct}")

    # 5. Handle Fix Logic
    cmd_extras = []
    if args.fix:
        car = toolchain.find_clang_apply_replacements()
        if not car:
            print(
                f"{COLOR_RED}[ERR] --fix requested but clang-apply-replacements not found.{COLOR_RESET}"
            )
            sys.exit(1)
        print(f"[LINT] Fixer tool:   {car}")

        if not args.force:
            print(
                f"\n{COLOR_YELLOW}{COLOR_BOLD}WARNING: You are about to automatically modify source code.{COLOR_RESET}"
            )
            if not confirm_action("Are you sure you want to proceed?"):
                print("Aborted.")
                sys.exit(0)
        cmd_extras.append("-fix")

    # 6. Build Command
    is_python = run_ct.suffix == ".py" or run_ct.name.endswith(".py")
    if not is_python:
        try:
            with open(run_ct, "r", encoding="utf-8", errors="ignore") as f:
                first_line = f.readline()
                if "python" in first_line:
                    is_python = True
        except Exception:
            pass

    cmd = [sys.executable, str(run_ct)] if is_python else [str(run_ct)]

    cmd.extend(
        [
            "-p",
            str(build_dir),
            "-j",
            str(ctx.cpu_count),
            "-quiet",
            "-source-filter",
            args.filter,
            "-warnings-as-errors",
            "*",
        ]
    )

    cmd.extend(["-header-filter", args.filter])
    cmd.extend(cmd_extras)

    # 7. Execute
    print(f"\n{COLOR_BOLD}>>> STARTING LINT PROCESS{COLOR_RESET}")
    returncode, issues = run_lint_streaming(cmd, env=env, verbose=args.verbose)

    # 8. Summary & Exit
    print_issue_summary_table(issues)
    success = returncode == 0 and len(issues) == 0
    ctx.print_summary(success, issue_count=len(issues))

    if not success:
        sys.exit(1)


if __name__ == "__main__":
    main()
