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
        self.tools["git"] = self._get_version(["git", "--version"])
        self.tools["ninja"] = self._get_version(["ninja", "--version"])

        # Probe compiler based on OS
        if os.name == "nt":
            self.tools["msvc"] = self._get_version(["cl"], stderr=True)
        else:
            self.tools["gcc"] = self._get_version(["gcc", "--version"])
            self.tools["clang"] = self._get_version(["clang", "--version"])

    def _get_version(self, cmd, stderr=False):
        try:
            # shell=True on Windows ensures we use the loaded environment PATH
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

            # Return just the first line (version number)
            return output.splitlines()[0] if output else "Unknown"
        except FileNotFoundError:
            return "Not found"
        except Exception:
            return "Error"

    def print_header(self):
        print("=" * 60)
        print(f"MAIASCAN PIPELINE - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"OS:     {platform.system()} {platform.release()}")
        print(f"Preset: {self.preset} | Cores: {self.cpu_count}")
        print("-" * 60)
        print(f"Tools:")
        for k, v in self.tools.items():
            print(f"  • {k:<6} : {v}")
        print("=" * 60)

    def print_summary(self, success):
        duration = time.time() - self.start_time
        status = "✨ SUCCESS" if success else "❌ FAILED"
        print("\n" + "=" * 60)
        print(f"PIPELINE SUMMARY")
        print(f"Result:   {status}")
        print(f"Duration: {duration:.2f}s")
        print("=" * 60 + "\n")


def run_command(cmd, env=None, check=True, quiet_tests=False):
    """Runs a command and streams output, optionally suppressing test noise."""
    print(f"\n[EXEC] {' '.join(cmd)}")

    # On Windows, shell=False usually works if executable is in path,
    # but strictly specific envs might prefer shell=True.
    # We stick to shell=False for safety unless strictly needed.
    use_shell = False

    if quiet_tests:
        # Specialized runner for ctest to reduce noise
        process = subprocess.Popen(
            cmd,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            shell=use_shell,
        )

        last_was_failed = False
        if process.stdout:
            for line in process.stdout:
                trimmed = line.strip()
                if not trimmed:
                    continue

                # CTest status lines: "1/5 Test #1: Name .... Passed"
                is_status_line = "Test #" in trimmed and (
                    "Passed" in trimmed or "Failed" in trimmed
                )

                if is_status_line:
                    if "Passed" in trimmed:
                        last_was_failed = False
                        continue  # Skip success lines
                    else:
                        print(trimmed)
                        last_was_failed = True
                        continue

                # Skip "Start 1: Name" lines
                if trimmed.startswith("Start ") and ":" in trimmed:
                    continue

                # Always print Summary lines
                is_summary = any(
                    x in trimmed
                    for x in [
                        "tests passed",
                        "tests failed",
                        "Total Test time",
                        "following tests FAILED",
                    ]
                )

                if is_summary:
                    print(trimmed)
                    last_was_failed = False
                elif last_was_failed:
                    # Print details if we are currently in a failure block
                    print(trimmed)
                elif not trimmed.startswith("Test project"):
                    # Suppress other noise
                    pass

        process.wait()
        return subprocess.CompletedProcess(cmd, process.returncode)

    # Standard execution for non-test commands
    result = subprocess.run(cmd, env=env, shell=use_shell)

    if check and result.returncode != 0:
        print(f"\n[ERR] Command failed with code {result.returncode}")
    return result


def find_vswhere():
    """Locates vswhere.exe using PATH or default install location."""
    # 1. Try PATH
    from_path = shutil.which("vswhere")
    if from_path:
        return Path(from_path)

    # 2. Try default location
    default_path = (
        Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)"))
        / "Microsoft Visual Studio/Installer/vswhere.exe"
    )
    if default_path.exists():
        return default_path

    return None


def get_msvc_environment():
    """Finds MSVC and loads environment variables via vcvars64.bat (Windows Only)."""

    # Skip entirely if not on Windows
    if os.name != "nt":
        return os.environ.copy()

    # If already active, return current env
    if "VCINSTALLDIR" in os.environ:
        print("[ENV] MSVC environment already active.")
        return os.environ.copy()

    vswhere_path = find_vswhere()
    if not vswhere_path:
        print("Error: vswhere.exe not found in PATH or standard locations.")
        sys.exit(1)

    try:
        install_path = subprocess.check_output(
            [str(vswhere_path), "-latest", "-property", "installationPath"], text=True
        ).strip()
    except subprocess.CalledProcessError:
        print("Error: Failed to find Visual Studio installation.")
        sys.exit(1)

    if not install_path:
        print("Error: No Visual Studio installation found.")
        sys.exit(1)

    vcvars_path = Path(install_path) / "VC/Auxiliary/Build/vcvars64.bat"
    if not vcvars_path.exists():
        print(f"Error: vcvars64.bat not found at {vcvars_path}")
        sys.exit(1)

    print(f"[ENV] Sourcing MSVC environment from {vcvars_path}")

    # Run vcvars and dump environment
    separator = "___ENV_SEPARATOR___"
    # Use && to ensure we only print separator if vcvars succeeds
    cmd = f'"{vcvars_path}" > nul && echo {separator} && set'

    output = subprocess.check_output(cmd, shell=True, text=True)

    if separator not in output:
        print("Error: Failed to capture environment from vcvars64.bat")
        sys.exit(1)

    _, env_block = output.split(separator, 1)

    new_env = os.environ.copy()
    for line in env_block.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            new_env[key] = value

    return new_env


def is_cache_stale(build_dir, root_dir):
    """Checks if the CMake cache is older than configuration files."""
    cache_file = build_dir / "CMakeCache.txt"

    # If build dir doesn't exist, we must configure
    if not build_dir.exists() or not cache_file.exists():
        return True

    cache_mtime = cache_file.stat().st_mtime
    configs = [
        root_dir / "CMakeLists.txt",
        root_dir / "CMakePresets.json",
        root_dir / "CMakeUserPresets.json",
        root_dir / "vcpkg.json",
    ]

    for config in configs:
        if config.exists() and config.stat().st_mtime > cache_mtime:
            print(f"[CONF] Cache is stale: {config.name} was modified.")
            return True
    return False


def main():
    parser = argparse.ArgumentParser(description="MaiaScan Build Script")
    parser.add_argument(
        "--preset",
        default=DEFAULT_PRESET,
        help=f"CMake preset (default: {DEFAULT_PRESET})",
    )
    parser.add_argument("--skip-tests", action="store_true", help="Skip running tests")
    parser.add_argument(
        "--reconfigure", action="store_true", help="Force CMake reconfiguration"
    )
    parser.add_argument(
        "--clean", action="store_true", help="Wipe build directory before starting"
    )
    args = parser.parse_args()

    # 1. Setup Environment
    env = get_msvc_environment()

    # 2. Metadata probe (now that env is ready)
    ctx = ProjectContext(args.preset, env=env)
    ctx.probe()
    ctx.print_header()

    if "Not found" in ctx.tools["cmake"]:
        print("Error: 'cmake' not found in PATH.")
        sys.exit(1)

    # 3. Resolve paths
    # Assumes script is in <root>/scripts/build.py or similar.
    # .resolve() ensures we have an absolute path.
    root_dir = Path(__file__).resolve().parent.parent
    build_dir = root_dir / "out" / "build" / args.preset

    # 4. Cleanup if requested
    if args.clean and build_dir.exists():
        print(f"[CLEAN] Removing build directory: {build_dir}")
        try:
            shutil.rmtree(build_dir)
        except Exception as e:
            print(f"[WARN] Failed to fully remove build directory: {e}")

    # 5. Configure
    should_configure = args.reconfigure or is_cache_stale(build_dir, root_dir)

    if should_configure:
        print(f"\n>>> STAGE: CONFIGURE")
        # We try once; if it fails, we clean and retry (auto-heal)
        res = run_command(["cmake", "--preset", args.preset], env=env, check=False)

        if res.returncode != 0:
            print("[QUIRK] Configuration failed. Attempting clean retry...")
            try:
                if build_dir.exists():
                    shutil.rmtree(build_dir, ignore_errors=True)
            except:
                pass
            res = run_command(["cmake", "--preset", args.preset], env=env, check=True)
            if res.returncode != 0:
                ctx.print_summary(False)
                sys.exit(res.returncode)
    else:
        print(
            f"\n[CONF] Configuration skipped (Cache is up to date). Use --reconfigure to force."
        )

    # 6. Build
    print(f"\n>>> STAGE: BUILD")
    # Ninja (default in presets) is parallel by default, but --parallel is safe.
    res = run_command(
        ["cmake", "--build", "--preset", args.preset, "--parallel"],
        env=env,
        check=False,
    )

    if res.returncode != 0:
        ctx.print_summary(False)
        sys.exit(res.returncode)

    # 7. Test
    success = True
    if not args.skip_tests:
        print(f"\n>>> STAGE: TEST")
        res = run_command(
            ["ctest", "--preset", args.preset, "--output-on-failure", "-j16"],
            env=env,
            check=False,
            quiet_tests=True,
        )
        success = res.returncode == 0

    ctx.print_summary(success)
    if not success:
        sys.exit(1)

    print("✨ PIPELINE SUCCESSFUL ✨")


if __name__ == "__main__":
    main()
