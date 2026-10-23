# Run from a PowerShell terminal where Python, CMake, Git and a C++17 toolchain are available.
# All arguments are forwarded to the shared installer; set CORGIREG_PYTHON to select Python.
$ErrorActionPreference = "Stop"
$pythonCommand = if ($env:CORGIREG_PYTHON) { $env:CORGIREG_PYTHON } else { "python" }
& $pythonCommand (Join-Path $PSScriptRoot "install.py") @args
exit $LASTEXITCODE
