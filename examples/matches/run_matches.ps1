param(
    [string]$Python = "python",
    [string]$OutputDir = (Join-Path $PSScriptRoot "../../build/example-matches")
)
& $Python (Join-Path $PSScriptRoot "run_matches.py") --output-dir $OutputDir
exit $LASTEXITCODE
