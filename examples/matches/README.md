# Matching examples

Install the base CLI and build the worker as described in [the CLI guide](../../doc/cli.md).
These JSON examples need no ONNX or StableHLO Python dependencies.

From the repository root, using the Python environment where CorGIReg is installed:

```sh
python examples/matches/run_matches.py --output-dir build/example-matches
```

Windows PowerShell, using the existing local environment:

```powershell
./examples/matches/run_matches.ps1 -Python ./build/cli-venv/Scripts/python.exe
```

The Python script works from any current directory, writes one JSON report per
case, checks the expected captures and exits with an error if verification fails.
Use `--engine /path/to/corgireg-match` to select a separately built worker.
Existing report files in the output directory are replaced.

| Case | Pattern | Expected full traversal from node 42 |
| --- | --- | --- |
| convolutions | `conv+->abs` | 42, 0, 9; `+` count is 2 |
| arithmetic | `add->multiply->negate->abs` | 42, 0, 9, 17 |
| no-match | `conv->tanh` | No matches |

The convolution graph also admits a suffix match starting at node 0. Reports
include all matches, source names, complete patterns and quantifier captures.

To run an individual example:

```sh
corgireg -i examples/matches/graphs/convolutions.json --node-library examples/library/nodes -R examples/matches/patterns/convolutions.txt -o build/convolutions.json
```

For real StableHLO input, use the same library:

```sh
corgireg -i examples/library/conv.mlir --node-library examples/library/nodes -R examples/library/patterns/conv-chain.txt -o build/stablehlo-convolutions.json
```

This last example requires the StableHLO extra. Replace the input with an ONNX
model containing convolutions to reuse that pattern with the ONNX extra.
