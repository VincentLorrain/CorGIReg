# Command-line matching

The public command is `corgireg`. Python reads external files and invokes the
existing C++ automaton through the `corgireg-match` worker. No inference runtime
or model execution is involved.

## Installation

Build the native worker from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

Use Python 3.12 or newer for the tested StableHLO extra (jaxlib 0.12). The base
JSON CLI requires Python 3.10 or newer. Install into a virtual environment:

```sh
python -m venv build/cli-venv
```

On Windows PowerShell:

```powershell
.\build\cli-venv\Scripts\Activate.ps1
python -m pip install -e ".[onnx,stablehlo]"
```

On Linux/macOS:

```sh
. build/cli-venv/bin/activate
python -m pip install -e ".[onnx,stablehlo]"
```

Use `python -m pip install -e .` for JSON only, or select just one format extra.
The installed command finds the worker in `build/` when using this editable
checkout. Alternatively pass `--engine /path/to/corgireg-match`, set
`CORGIREG_ENGINE`, or put the worker on `PATH`. The CMake install target installs
the worker to `bin`; Python packaging installs the separate public command.

## Usage

```sh
corgireg -i examples/cli/network.json -r "A+->B" -o build/matches.json
corgireg -i model.onnx --format onnx -r "Relu+->Neg" -o build/onnx-matches.json
corgireg -i examples/cli/network.mlir --format stablehlo --regex-file examples/cli/pattern.txt -o build/hlo-matches.json
```

| Option | Meaning |
| --- | --- |
| `-i`, `--input` | Required source graph file. |
| `-f`, `--format` | `json`, `onnx` or `stablehlo`; inferred from `.json`, `.onnx`, `.mlir`, `.stablehlo` if omitted. |
| `-r`, `--regex` | Pattern passed directly. Quote it to protect `>`, `$`, `*` and spaces from the shell. |
| `-R`, `--regex-file` | UTF-8 text file containing one complete pattern. Mutually exclusive with `-r`. |
| `-o`, `--output` | JSON file path; `-` or omission writes to stdout. The parent directory must exist. |
| `--function` | StableHLO function name; default is `main`, or the sole function. |
| `--engine` | Native worker path override. |
| `--max-starts` | Maximum number of starting tuples; default 100000. |
| `--timeout` | Maximum matching time in seconds; default 60. |

In PowerShell, use single quotes around a pattern containing `$` to avoid
variable expansion. Newlines and tabs in a pattern file count as whitespace.
The complete original file text is retained in the report. A pattern file is
not a list of independent queries: `;` retains its existing graph-pattern
composition meaning.

The CLI searches all possible starting nodes. A pattern with multiple starts
searches their Cartesian product, subject to `--max-starts`. Exceeding a budget
is an error, not a silently truncated report. An empty graph is searched with
absent starts so nullable patterns can succeed. Matching follows the existing
UNIQUE/COMMON/STRICTCOMMON semantics: a plain operation key requires at most one
parent and one child; use the graph language's `#`/`$` common-node syntax when
appropriate for junctions.

## Operation names and formats

Predicates match operation types exactly. Characters outside ASCII letters,
digits and underscore become underscores in predicate names:
`stablehlo.negate` becomes `stablehlo_negate`. The report's `graph.nodes` contains
both the original `op` and the corresponding `predicate`. Alias collisions are
rejected. Node display names do not determine which predicate matches.

**JSON** uses an operation graph with `nodes` and `edges`:

```json
{
  "nodes": [
    {"id": 42, "op": "A", "name": "producer"},
    {"id": 9, "op": "B", "name": "consumer"}
  ],
  "edges": [[42, 9]]
}
```

IDs are unsigned 64-bit integers, including zero; duplicates and unknown edge
endpoints are errors. `name` defaults to the decimal ID. Repeated edges collapse
to a single adjacency relation. JSON consumers must preserve integer precision
when using IDs above 2^53.

**ONNX** uses the [official ONNX reader](https://onnx.ai/onnx/api/serialization.html).
IDs are zero-based indices in `model.graph.node`; display names are the original
node names, or `node_INDEX` when unnamed. Value producers and consumers determine
edges. Graph inputs and initializers are boundaries, not extra operation nodes;
tensor weights stored externally are not loaded. Node inputs/outputs are retained
in the report to identify source values. Default-domain op names are unchanged;
custom-domain predicates include the domain. Local function calls remain opaque
operations. Nested graph attributes such as `If`/`Loop` subgraphs are currently
rejected explicitly. This is a topology importer, not a full ONNX model checker.

**StableHLO/OpenXLA** uses the native MLIR bindings shipped in jaxlib; see also
the [upstream StableHLO Python bindings](https://github.com/openxla/stablehlo#python).
This version accepts textual MLIR modules and selects one single-block function.
IDs are operation indices within that function's block. SSA producer/use links
determine edges; block arguments and `func.return` are boundaries. The report
retains the function, operation type, location and canonical SSA result names.
MLIR can rename SSA values while parsing: use the function and operation index
for source identity rather than expecting verbatim textual SSA spelling.
Nested regions (including reductions and control flow), other dialect operations
and interprocedural calls are rejected. Raw XLA HLO dumps/protobuf and portable
StableHLO bytecode are not supported by this reader.

## Results and exit status

The JSON report has `schema_version`, `source`, `format`, `query`, `graph` and
`matches`. Each match contains unique `node_ids`, its `starts` and contributing
`walks`. Walks contain ordered named node captures and quantifier invocations
with `index`, `symbol`, `count`, `parent`, `begin`, `end`, `iteration_ends`.
See [match traces](match-traces.md) for the interpretation of nested counts and
capture ranges. Result order is not a ranking. Matching can return overlapping
matches and different quantifier allocations for the same node set.

The JSON example with `A+->B` includes a match starting at 42 with nodes
`[42, 0, 9]` in execution order and quantifier 1, `+`, count 2. It also finds
the shorter match starting at 0 with count 1.

Exit code 0 means a valid report, including `matches: []` when no pattern
matches. Exit code 2 indicates invalid arguments, import/matching errors or a
timeout. Diagnostics go to stderr. An existing output file is replaced only
after a complete successful run; the input graph and pattern file cannot be
used as output paths.

## Verification

Core CLI tests run under CTest when a Python interpreter is available. To enable
the actual ONNX/MLIR import tests, configure with a Python containing both extras:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCORGIREG_IMPORT_PYTHON="$PWD/build/cli-venv/Scripts/python.exe"
cmake --build build --parallel
ctest --test-dir build --output-on-failure --timeout 30
```

On Linux/macOS use the corresponding `build/cli-venv/bin/python` absolute path.
The import tests are required when this option is set; missing dependencies fail
the test rather than silently skipping it. Direct invocation is also available:

```sh
python -m unittest discover -s test/cli_python -p "test_*.py" -v
```

`APP` uses explicit source graphs and bounded, seeded generated chains. It checks
the complete original pattern corpus, empty inputs, capture identities, nested
quantifier counts and negative cases. It does not assume that every randomly
generated syntactically valid pattern must match a graph produced by the IR
example generator. The full development suite, including `APP`, passes.
