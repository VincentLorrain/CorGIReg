# Installation scripts

Run `install.ps1` on Windows or `bash installation/install.sh` on Linux. Both
invoke `install.py`, so installation and validation follow the same steps:

1. Check Python, Git, CMake and CTest availability.
2. Create or reuse a local virtual environment and install the CLI there.
3. Configure a Debug C++ build, compile it and install the native worker.
4. Require discovered tests and run the complete configured CTest suite.
5. Run optional reader tests and verify the matching examples using the installed worker.
6. Check that the installed CLI finds its worker without `CORGIREG_ENGINE`.

Any failed command stops installation and returns a nonzero exit status. An
interrupted installation can be rerun; existing directories are not cleared.
Successful earlier steps are retained when a later step fails.

## Prerequisites

- Python 3.10+ for JSON, Python 3.12+ for the tested StableHLO extra.
- CMake 3.20+, Git, and a C++17 compiler with its build tool on PATH.
- Windows: the tested compiler is MinGW-w64 (`g++`, `mingw32-make`). Its runtime
  DLLs must remain discoverable on PATH when using the native executable.
- Linux: GCC or Clang, Make or Ninja, and the Python venv/ensurepip components.
- Network access for pip packages and CMake's fmt/Catch2 downloads.

Install system prerequisites through your system's package manager or toolchain
installer first. These scripts do not install system packages or request admin
privileges. They create a source-development installation, not a standalone
redistributable executable. Optional reader wheels must be available for your
Python version and platform; use `--formats json` if they are unavailable.

## Options

All options work with either launcher and with `python installation/install.py`.

| Option | Default | Purpose |
| --- | --- | --- |
| `--formats` | `json` | `json`, `onnx`, `stablehlo`, or `all` |
| `--venv` | `<repo>/build/install-venv` | Local Python environment and installed worker |
| `--build-dir` | `<repo>/build/install-build` | CMake build directory |
| `--jobs` | `2` | Parallel compilation jobs |
| `--generator` | CMake default, or detected MinGW on Windows | Explicit CMake generator |
| `--help` | | Print options without installing |

The default paths are relative to the repository, even when launching elsewhere.
Explicit relative paths are resolved against the current working directory.
Tests also use `<repo>/build/` for temporary fixtures, even with a custom build directory.
Paths with spaces must be quoted. Reuse the same options when updating an
installation. Changing `--formats` does not uninstall previously added readers.
The generated CLI lives in `<venv>/Scripts/corgireg.exe` on Windows or
`<venv>/bin/corgireg` on Linux. Its worker is in `<venv>/bin/` on both systems.

Windows example with custom directories:

```powershell
.\installation\install.ps1 --formats all --venv "build/my environment" --build-dir build/my-build --jobs 4
```

Linux:

```sh
bash installation/install.sh --formats all --venv "build/my environment" --build-dir build/my-build --jobs 4
```

Select Python for this shell session:

```powershell
$env:CORGIREG_PYTHON = 'C:\Path\To\Python312\python.exe'
.\installation\install.ps1 --formats all
```

```sh
CORGIREG_PYTHON=python3.12 bash installation/install.sh --formats all
```

An existing virtual environment keeps its original interpreter. Use a new
`--venv` directory when changing Python. The installer rejects nonempty
directories that are not virtual environments.

## Troubleshooting and updates

- If PowerShell execution policy blocks the launcher, run
  `python installation/install.py` with the same options instead.
- If CMake cannot find a compiler, fix the toolchain's PATH or select the
  appropriate developer terminal. `--generator` selects a build system; it
  does not install a compiler.
- When changing compilers or CMake generators, choose a fresh `--build-dir`.
  Existing CMake caches are reused, never deleted automatically.
- For dependency download errors, check network/proxy access. A partial install
  is not reported as success; rerun after fixing the connection.
- The package is editable. Keep the checkout at its original location; rerun the
  installer after C++ changes so the installed worker is refreshed.

The tests run on the build-tree worker, while example assertions run on the
installed worker. Reports are written under `<build-dir>/example-matches/`.
`--formats all` includes both real ONNX and StableHLO integration suites;
selecting just one reader runs its tests separately after CTest.
