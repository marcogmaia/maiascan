import os
import sys
import subprocess
import argparse
import shutil
import time
import platform
import re
from pathlib import Path
from datetime import datetime
from dataclasses import dataclass
from typing import List, Optional, Dict, Tuple, Pattern

# --- Configuration ---
DEFAULT_PRESET = "windows-release" if os.name == "nt" else "linux-debug"
DEFAULT_SOURCE_FILTER = ".*src.*"  # Default regex for source files

# ANSI Colors
COLOR_RESET = "\033[0m"
COLOR_RED = "\033[91m"
COLOR_YELLOW = "\033[93m"
COLOR_CYAN = "\033[96m"
COLOR_GREEN = "\033[92m"
COLOR_BOLD = "\033[1m"
COLOR_DIM = "\033[2m"


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
        return (
            f"{COLOR_BOLD}{self.file}:{self.line}:{self.col}{COLOR_RESET}: "
            f"{color}{self.severity}{COLOR_RESET}: {self.message} "
            f"[{COLOR_CYAN}{self.check}{COLOR_RESET}]"
        )


class OutputParser:
    """Parses clang-tidy output streams."""

    def __init__(self):
        # Regex: path:line:col: severity: message [check-name]
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
            # We track errors and warnings; notes are usually attached to previous issues
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
            # shell=True only on Windows to find built-ins or bat files easier
            use_shell = os.name == "nt"
            res = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                env=self.env,
                shell=use_shell,
            )
            output = res.stderr if stderr else res.stdout
            if not output and not stderr:
                output = res.stderr
            return output.splitlines()[0] if output else "Unknown"
        except (FileNotFoundError, PermissionError):
            return "Not found"

    def find_run_clang_tidy(self) -> Optional[Path]:
        """Locates the run-clang-tidy python script or executable."""
        # 1. Check PATH
        path_val = self.env.get("PATH", "")
        search_names = ["run-clang-tidy", "run-clang-tidy.py", "run-clang-tidy-14.py"]

        for p in path_val.split(os.pathsep):
            for name in search_names:
                candidate = Path(p) / name
                if candidate.exists():
                    return candidate

        # 2. Check relative to clang-tidy binary
        clang_tidy_path = shutil.which("clang-tidy", path=path_val)
        if clang_tidy_path:
            ct_bin = Path(clang_tidy_path).parent
            # Same dir or ../share/clang
            search_dirs = [ct_bin, ct_bin.parent / "share" / "clang"]
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
        """Locates clang-apply-replacements for fixing issues."""
        path_val = self.env.get("PATH", "")
        # 1. Try PATH
        from_path = shutil.which("clang-apply-replacements", path=path_val)
        if from_path:
            return Path(from_path)

        # 2. Relative to clang-tidy
        clang_tidy_path = shutil.which("clang-tidy", path=path_val)
        if clang_tidy_path:
            candidate = Path(clang_tidy_path).parent / "clang-apply-replacements"
            if os.name == "nt":
                candidate = candidate.with_suffix(".exe")
            if candidate.exists():
                return candidate
        return None


class ProjectContext:
    """Manages project state, reporting, and execution."""

    def __init__(self, preset: str, root_dir: Path, toolchain: Toolchain):
        self.preset = preset
        self.root_dir = root_dir
        self.toolchain = toolchain
        self.start_time = time.time()
        self.cpu_count = os.cpu_count() or 1

    def print_header(self):
        print(f"{COLOR_BOLD}{'=' * 60}")
        print(
            f"MAIASCAN LINT PIPELINE - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
        )
        print(f"{'=' * 60}{COLOR_RESET}")
        print(f"OS:       {platform.system()} {platform.release()}")
        print(f"Preset:   {self.preset}")
        print(f"Root:     {self.root_dir}")
        print(f"Cores:    {self.cpu_count}")
        print(f"{'-' * 60}")
        print(f"Tools:")
        for k, v in self.toolchain.tools.items():
            print(f" • {k:<12} : {v}")
        print(f"{'=' * 60}{COLOR_RESET}\n")

    def print_summary(self, success: bool, issue_count: int):
        duration = time.time() - self.start_time
        status = (
            f"{COLOR_GREEN}✨ SUCCESS{COLOR_RESET}"
            if success
            else f"{COLOR_RED}❌ FAILED{COLOR_RESET}"
        )

        print("\n" + "=" * 60)
        print(f"{COLOR_BOLD}LINT SUMMARY{COLOR_RESET}")
        print(f"Result:   {status}")
        print(f"Issues:   {issue_count}")
        print(f"Duration: {duration:.2f}s")
        print("=" * 60 + "\n")


def get_msvc_environment() -> Dict[str, str]:
    """Finds MSVC and loads environment variables via vcvars64.bat (Windows Only)."""
    if os.name != "nt":
        return os.environ.copy()
    if "VCINSTALLDIR" in os.environ:
        return os.environ.copy()

    # Locate vswhere
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

    try:
        install_path = subprocess.check_output(
            [str(vswhere_path), "-latest", "-property", "installationPath"], text=True
        ).strip()
    except subprocess.CalledProcessError:
        return os.environ.copy()

    if not install_path:
        return os.environ.copy()

    vcvars_path = Path(install_path) / "VC/Auxiliary/Build/vcvars64.bat"
    if not vcvars_path.exists():
        return os.environ.copy()

    separator = "___ENV_SEPARATOR___"
    # Run vcvars and dump environment
    cmd = f'"{vcvars_path}" > nul && echo {separator} && set'
    try:
        output = subprocess.check_output(cmd, shell=True, text=True)
        if separator not in output:
            return os.environ.copy()
        _, env_block = output.split(separator, 1)
        new_env = os.environ.copy()
        for line in env_block.splitlines():
            if "=" in line:
                key, value = line.split("=", 1)
                new_env[key] = value
        return new_env
    except Exception:
        return os.environ.copy()


def run_lint_streaming(
    cmd: List[str], env: Dict[str, str], verbose: bool = False
) -> Tuple[int, List[LintIssue]]:
    """Runs clang-tidy and parses output in real-time."""
    if verbose:
        print(f"{COLOR_DIM}[CMD] {' '.join(cmd)}{COLOR_RESET}")

    parser = OutputParser()

    # Use Popen for streaming
    process = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,  # Merge stderr to stdout for sequential processing
        env=env,
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,  # Line buffered
    )

    if process.stdout:
        for line in process.stdout:
            # Parse line for known issues
            issue = parser.parse_line(line)
            if issue:
                print(str(issue))
            else:
                # Filter noise, print useful status info
                stripped = line.strip()
                if not stripped:
                    continue
                # Common clang-tidy status messages we might want to see or hide
                if "warnings generated" in stripped:
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

    # Group by check name
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
    """Asks user for confirmation."""
    while True:
        response = (
            input(f"{COLOR_YELLOW}{message} [y/N]: {COLOR_RESET}").lower().strip()
        )
        if response in ["y", "yes"]:
            return True
        if response in ["", "n", "no"]:
            return False


def main():
    parser = argparse.ArgumentParser(description="MaiaScan Lint Script")
    parser.add_argument(
        "--preset",
        default=DEFAULT_PRESET,
        help=f"CMake preset (default: {DEFAULT_PRESET})",
    )
    parser.add_argument(
        "--root",
        default=os.getcwd(),
        help="Project root directory (default: CWD)",
    )
    parser.add_argument(
        "--fix",
        action="store_true",
        help="Apply suggested fixes to enabled checks",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Skip confirmation prompt for --fix",
    )
    parser.add_argument(
        "--filter",
        default=DEFAULT_SOURCE_FILTER,
        help=f"Regex for source files to check (default: {DEFAULT_SOURCE_FILTER})",
    )
    parser.add_argument(
        "--exclude",
        help="Regex header/source filter to EXCLUDE (passed to -header-filter or internal logic)",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print full commands and debug info",
    )
    args = parser.parse_args()

    # 1. Setup Environment
    env = get_msvc_environment()
    toolchain = Toolchain(env)
    toolchain.probe()

    root_dir = Path(args.root).resolve()
    build_dir = root_dir / "out" / "build" / args.preset
    compile_commands = build_dir / "compile_commands.json"

    ctx = ProjectContext(args.preset, root_dir, toolchain)
    ctx.print_header()

    # 2. Ensure compile_commands.json exists
    if not compile_commands.exists():
        print(f"[LINT] compile_commands.json not found in {build_dir}. Configuring...")
        res = subprocess.run(
            ["cmake", "--preset", args.preset], env=env, cwd=str(root_dir)
        )
        if res.returncode != 0:
            print(f"{COLOR_RED}[ERR] CMake configuration failed.{COLOR_RESET}")
            sys.exit(res.returncode)

    if not compile_commands.exists():
        print(
            f"{COLOR_RED}[ERR] Failed to generate compile_commands.json at {compile_commands}{COLOR_RESET}"
        )
        sys.exit(1)

    # 3. Find run-clang-tidy
    run_ct = toolchain.find_run_clang_tidy()
    if not run_ct:
        print(
            f"{COLOR_RED}[ERR] run-clang-tidy not found. Please ensure LLVM is installed.{COLOR_RESET}"
        )
        sys.exit(1)

    print(f"[LINT] Linter script: {run_ct}")

    # 4. Handle Fix Logic
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

    # 5. Build Command
    # Determine execution method (python vs binary)
    is_python = run_ct.suffix == ".py" or run_ct.name.endswith(".py")
    if not is_python:
        # Inspect shebang for safety
        try:
            with open(run_ct, "r", encoding="utf-8", errors="ignore") as f:
                first_line = f.readline()
                if "python" in first_line:
                    is_python = True
        except Exception:
            pass

    cmd = [sys.executable, str(run_ct)] if is_python else [str(run_ct)]

    # Core arguments
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
            "*",  # Strict mode
        ]
    )

    # Header filter (optional, usually matches source filter or includes everything)
    # If the user provided an exclude regex, we can't easily pass it to run-clang-tidy
    # natively without modifying the script, but we can refine the header filter.
    cmd.extend(["-header-filter", args.filter])

    cmd.extend(cmd_extras)

    # 6. Execute
    print(f"\n{COLOR_BOLD}>>> STARTING LINT PROCESS{COLOR_RESET}")
    returncode, issues = run_lint_streaming(cmd, env=env, verbose=args.verbose)

    # 7. Summary & Exit
    print_issue_summary_table(issues)

    # Success logic: Must have return code 0 AND no parsed errors
    # Note: run-clang-tidy might return 0 even if it found warnings,
    # but we used -warnings-as-errors, so it should return non-zero on issues.
    success = returncode == 0 and len(issues) == 0
    ctx.print_summary(success, issue_count=len(issues))

    if not success:
        sys.exit(1)


if __name__ == "__main__":
    main()
