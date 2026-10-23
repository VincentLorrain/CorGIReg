"""Build, install and test CorGIReg in a local Python virtual environment."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import venv

ROOT = Path(__file__).resolve().parents[1]


def run(command, **kwargs):
    print("+ " + subprocess.list2cmdline([str(item) for item in command]), flush=True)
    return subprocess.run([str(item) for item in command], check=True, **kwargs)


def install(args):
    minimum = (3, 12) if args.formats in ("stablehlo", "all") else (3, 10)
    if sys.version_info < minimum:
        raise ValueError(f"This selection requires Python {minimum[0]}.{minimum[1]} or newer")
    for tool in ("cmake", "ctest", "git"):
        if not shutil.which(tool):
            raise ValueError(f"Missing prerequisite on PATH: {tool}")
    build = args.build_dir.resolve()
    environment = args.venv.resolve()
    if build == ROOT or environment == ROOT or build == environment:
        raise ValueError("Build and virtual environment directories must be distinct from each other and the source root")
    python = environment / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    if environment.exists() and any(environment.iterdir()) and not (environment / "pyvenv.cfg").is_file():
        raise ValueError(f"Refusing to use a non-virtual-environment directory: {environment}")
    if not python.is_file():
        print(f"Creating virtual environment: {environment}", flush=True)
        venv.EnvBuilder(with_pip=True).create(environment)
    run([python, "-c", f"import sys; sys.exit(0 if sys.version_info >= {minimum!r} else 'Virtual environment Python is too old')"])
    extras = {"json": "", "onnx": "[onnx]", "stablehlo": "[stablehlo]", "all": "[onnx,stablehlo]"}[args.formats]
    run([python, "-m", "pip", "install", "-e", str(ROOT) + extras])
    configure = ["cmake", "-S", ROOT, "-B", build, "-DCMAKE_BUILD_TYPE=Debug",
                 f"-DPython3_EXECUTABLE={python.as_posix()}",
                 f"-DCORGIREG_IMPORT_PYTHON={python.as_posix() if args.formats == 'all' else ''}"]
    generator = args.generator
    if not generator and not (build / "CMakeCache.txt").exists() and os.name == "nt":
        if shutil.which("g++") and shutil.which("mingw32-make"):
            generator = "MinGW Makefiles"
    if generator:
        configure.extend(["-G", generator])
    run(configure)
    run(["cmake", "--build", build, "--config", "Debug", "--parallel", args.jobs])
    run(["cmake", "--install", build, "--config", "Debug", "--prefix", environment, "--component", "Runtime"])
    engine = environment / "bin" / ("corgireg-match.exe" if os.name == "nt" else "corgireg-match")
    if not engine.is_file():
        raise ValueError(f"Native worker was not installed: {engine}")
    # Existing integration fixtures use <source>/build for their temporary files,
    # even when the user chooses a build directory outside the checkout.
    (ROOT / "build").mkdir(exist_ok=True)
    tests = run(["ctest", "--test-dir", build, "-C", "Debug", "--show-only=json-v1"],
                capture_output=True, text=True)
    if not json.loads(tests.stdout).get("tests"):
        raise ValueError("No tests were discovered; refusing to report installation success")
    run(["ctest", "--test-dir", build, "-C", "Debug", "--output-on-failure"])
    # CTest's optional aggregate requires both readers; test a single selected reader separately.
    if args.formats in ("onnx", "stablehlo"):
        run([python, "-m", "unittest", "discover", "-s", ROOT / "test/cli_python",
             "-p", f"test_import_{args.formats}.py"], env={**os.environ, "CORGIREG_ENGINE": str(engine)})
    reports = build / "example-matches"
    run([python, ROOT / "examples/matches/run_matches.py", "--output-dir", reports, "--engine", engine])
    run([python, ROOT / "examples/matches/run_rules.py", "--output", reports / "conditional.json", "--engine", engine])
    command = environment / ("Scripts/corgireg.exe" if os.name == "nt" else "bin/corgireg")
    # Verify that the installed CLI finds its own worker without an environment override.
    clean_env = {key: value for key, value in os.environ.items() if key != "CORGIREG_ENGINE"}
    run([command, "-i", ROOT / "examples/cli/network.json", "-r", "A+->B",
         "-o", reports / "installed-cli.json"], env=clean_env)
    print(f"\nInstallation and tests succeeded.\nCLI: {command}\nPython: {python}\nReports: {reports}")
    print("Keep this source checkout: the Python package is installed in editable mode.")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--venv", type=Path, default=ROOT / "build/install-venv")
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/install-build")
    parser.add_argument("--formats", choices=["json", "onnx", "stablehlo", "all"], default="json")
    parser.add_argument("--generator", help="CMake generator override; use a new build directory when changing it")
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args(argv)
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    try:
        install(args)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Installation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
