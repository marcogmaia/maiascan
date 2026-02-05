import os
import sys
import subprocess
import argparse
import shutil
import time
import platform
from pathlib import Path
from datetime import datetime

# --- Configuration ---
DEFAULT_PRESET = "windows-release" if os.name == "nt" else "linux-debug"


class ProjectContext:
    """Gathers metadata about the environment for LLM consumption and user debug."""

    def __init__(self, preset, env=None):
        self.preset = preset
        self.env = env
        self.start_time = time.time()
        self.cpu_count = os.cpu_count() or 1
        self.tools = {}

    def probe(self):
        """Checks for the existence and version of critical build tools."""
        self.tools["cmake"] = self._get_version(["cmake", "--version"])
        self.tools["clang-tidy"] = self._get_version(["clang-tidy", "--version"])

        # Probe compiler based on OS
        if os.name == "nt":
            self.tools["msvc"] = self._get_version(["cl"], stderr=True)
        else:
            self.tools["clang"] = self._get_version(["clang", "--version"])

    def _get_version(self, cmd, stderr=False):
        try:
            use_shell = os.name == "nt"
            res = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=False,
                env=self.env,
                shell=use_shell,
            )
            output = res.stderr if stderr else res.stdout
            if not output and not stderr:
                output = res.stderr

            return output.splitlines()[0] if output else "Unknown"
        except FileNotFoundError:
            return "Not found"
        except Exception:
            return "Error"

    def print_header(self):
        print("=" * 60)
        print(
            f"MAIASCAN LINT PIPELINE - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
        )
        print(f"OS:     {platform.system()} {platform.release()}")
        print(f"Preset: {self.preset} | Cores: {self.cpu_count}")
        print("-" * 60)
        print(f"Tools:")
        for k, v in self.tools.items():
            print(f"  • {k:<12} : {v}")
        print("=" * 60)

    def print_summary(self, success):
        duration = time.time() - self.start_time
        status = "✨ SUCCESS" if success else "❌ FAILED"
        print("\n" + "=" * 60)
        print(f"LINT SUMMARY")
        print(f"Result:   {status}")
        print(f"Duration: {duration:.2f}s")
        print("=" * 60 + "\n")


def run_command(cmd, env=None, check=True):
    """Runs a command and streams output."""
    print(f"\n[EXEC] {' '.join(cmd)}")
    result = subprocess.run(cmd, env=env, shell=False)
    if check and result.returncode != 0:
        print(f"\n[ERR] Command failed with code {result.returncode}")
    return result


def find_vswhere():
    """Locates vswhere.exe using PATH or default install location."""
    from_path = shutil.which("vswhere")
    if from_path:
        return Path(from_path)
    default_path = (
        Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)"))
        / "Microsoft Visual Studio/Installer/vswhere.exe"
    )
    if default_path.exists():
        return default_path
    return None


def get_msvc_environment():
    """Finds MSVC and loads environment variables via vcvars64.bat (Windows Only)."""
    if os.name != "nt":
        return os.environ.copy()
    if "VCINSTALLDIR" in os.environ:
        return os.environ.copy()
    vswhere_path = find_vswhere()
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


def find_run_clang_tidy(env):
    """Finds run-clang-tidy script."""
    # 1. Try PATH
    path_val = env.get("PATH", "")
    for p in path_val.split(os.pathsep):
        for name in ["run-clang-tidy", "run-clang-tidy.py"]:
            candidate = Path(p) / name
            if candidate.exists():
                return candidate

    # 2. Try relative to clang-tidy
    clang_tidy_path = shutil.which("clang-tidy", path=path_val)
    if clang_tidy_path:
        ct_bin = Path(clang_tidy_path).parent
        # Try same dir
        for name in ["run-clang-tidy", "run-clang-tidy.py"]:
            candidate = ct_bin / name
            if candidate.exists():
                return candidate
        # Try share/clang (often one level up from bin)
        share_dir = ct_bin.parent / "share" / "clang"
        for name in ["run-clang-tidy", "run-clang-tidy.py"]:
            candidate = share_dir / name
            if candidate.exists():
                return candidate

    # 3. Common locations on Windows
    if os.name == "nt":
        llvm_paths = [
            Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "LLVM",
            Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)"))
            / "LLVM",
        ]
        for base in llvm_paths:
            for sub in ["bin", "share/clang"]:
                for name in ["run-clang-tidy", "run-clang-tidy.py"]:
                    candidate = base / sub / name
                    if candidate.exists():
                        return candidate
    return None


def main():
    parser = argparse.ArgumentParser(description="MaiaScan Lint Script")
    parser.add_argument(
        "--preset",
        default=DEFAULT_PRESET,
        help=f"CMake preset (default: {DEFAULT_PRESET})",
    )
    args = parser.parse_args()

    # 1. Setup Environment
    env = get_msvc_environment()

    # 2. Metadata probe
    ctx = ProjectContext(args.preset, env=env)
    ctx.probe()
    ctx.print_header()

    # 3. Resolve paths
    root_dir = Path(__file__).resolve().parent.parent
    build_dir = root_dir / "out" / "build" / args.preset
    compile_commands = build_dir / "compile_commands.json"

    # 4. Ensure compile_commands.json exists
    if not compile_commands.exists():
        print(f"[LINT] compile_commands.json not found in {build_dir}. Configuring...")
        res = subprocess.run(["cmake", "--preset", args.preset], env=env, cwd=root_dir)
        if res.returncode != 0:
            print("[ERR] CMake configuration failed.")
            sys.exit(res.returncode)

    if not compile_commands.exists():
        print(f"[ERR] Failed to generate compile_commands.json at {compile_commands}")
        sys.exit(1)

    # 5. Find run-clang-tidy
    run_ct = find_run_clang_tidy(env)
    if not run_ct:
        print("[ERR] run-clang-tidy not found. Please ensure LLVM is installed.")
        sys.exit(1)

    print(f"[LINT] Using {run_ct}")

    # 6. Build Command
    # On Windows, we should use sys.executable if it's a python script
    is_python = run_ct.suffix == ".py"
    if not is_python:
        # Check first line for shebang
        try:
            with open(run_ct, "r", encoding="utf-8", errors="ignore") as f:
                if "python" in f.readline():
                    is_python = True
        except:
            pass

    cmd = [sys.executable, str(run_ct)] if is_python else [str(run_ct)]
    cmd.extend(
        [
            "-p",
            str(build_dir),
            "-quiet",
            "-source-filter",
            ".*src.*",
            "-warnings-as-errors",
            "*",
        ]
    )

    # 7. Execute
    print(f"\n>>> STAGE: LINT")
    res = run_command(cmd, env=env, check=False)

    ctx.print_summary(res.returncode == 0)
    if res.returncode != 0:
        sys.exit(res.returncode)

    print("✨ LINTING SUCCESSFUL ✨")


if __name__ == "__main__":
    main()
