# CorGIReg - Core Graph Intersect Regex

[![CorGIReg CI](https://github.com/VincentLorrain/CorGIReg/actions/workflows/ci.yml/badge.svg)](https://github.com/VincentLorrain/CorGIReg/actions/workflows/ci.yml)

CorGIReg is an C++ library designed to apply regular expression operations on graph structures. This library extends traditional text-based regular expressions to graphs, allowing for powerful and flexible pattern matching in complex data structures.

See the [documentation index](doc/README.md) for the concepts, pattern language,
input IRs, node definitions, complete match JSON format, C++ adapters and tooling.

The initial integrated source milestone is [V0](doc/releases/V0.md), available
through the Git tag `V0`.

## Installation on Windows and Linux

Clone this repository, open a terminal in its root directory, then run one of
the scripts in [`installation/`](installation/README.md).

**Prerequisites:** Git, CMake 3.20 or newer, Python 3.10 or newer (3.12 for
StableHLO), and a C++17 compiler with its build tool on `PATH`. On Windows,
MinGW-w64 with `g++` and `mingw32-make` is the tested toolchain. On Linux, use
GCC or Clang with Make or Ninja and Python's `venv` support. Internet access is
needed to download Python dependencies, fmt and Catch2 on the first run.

Windows PowerShell:

```powershell
.\installation\install.ps1
```

Linux:

```sh
bash installation/install.sh
```

These commands install JSON graph support, compile the C++ engine, run the
test suite and verify the matching examples. To also install **ONNX and
StableHLO**, use Python 3.12 or newer and add `--formats all`:

```powershell
.\installation\install.ps1 --formats all
```

```sh
bash installation/install.sh --formats all
```

Everything is installed locally, without administrator privileges or global
Python package changes:

| Location | Contents |
| --- | --- |
| `build/install-venv/` | Python environment, CLI and native worker |
| `build/install-build/` | CMake build, dependencies and test binaries |
| `build/install-build/example-matches/` | Verified matching reports |

Run the installed CLI without activating the environment:

```powershell
.\build\install-venv\Scripts\corgireg.exe -i examples/cli/network.json -r 'A+->B' -o build/matches.json
```

```sh
./build/install-venv/bin/corgireg -i examples/cli/network.json -r 'A+->B' -o build/matches.json
```

The scripts can be rerun after source changes to rebuild and retest. Keep the
checkout: installation uses editable Python sources. See the
[installation guide](installation/README.md) for custom directories, Python
selection, prerequisites and troubleshooting.

To run patterns directly on an existing graph through a minimal read-only
interface, see [Graph adapters](doc/graph-adapters.md). Results expose graph-local
node IDs, with an adapter for resolving existing `NodeNN` objects.

The [command-line guide](doc/cli.md) covers JSON, ONNX and textual StableHLO input:

```sh
corgireg -i network.onnx -r "Relu+->Neg" -o matches.json
corgireg -i network.mlir --format stablehlo --regex-file pattern.txt -o matches.json
```

Reports include the complete pattern, original operation IDs, ordered captures
and the actual quantifier counts. Build the native worker and install the Python
front end as described in the guide before using these commands.

## Performance tracking

Record reproducible native timings and a JSON report tagged with the Git revision:

```sh
python benchmarks/track.py run
```

Reports are saved in `build/performance/reports/`. Compare two runs with
`python benchmarks/track.py compare reference.json current.json`. The
[benchmark guide](benchmarks/README.md) explains reference files, compatibility
checks, slowdown thresholds and CI artifacts. Performance measurements use a
separate Release build; functional tests retain their own checks.

# Graph-Regex Token Types

CorGIReg extends conventional regular-expression syntax with tokens that describe how a node condition should be interpreted inside a transition graph.  The lexer recognises three flavours of "key" tokens, each mapping to a distinct transition type:

| Token | Lexical form | Transition type | Meaning |
|-------|--------------|-----------------|---------|
| `KEY` | `A`, `foo_1` | `TransitionTypes::UNIQUE` | A standard node-condition key; the transition succeeds when the node identified by the key matches. |
| `CKEY` | `A#`, `foo#2` | `TransitionTypes::COMMON` | A *common* key.  The `#` suffix groups multiple transitions that should share the same common anchor, but without enforcing that every adjacent branch of the common node must be consumed. |
| `SCKEY` | `A$`, `foo$3` | `TransitionTypes::STRICTCOMMON` | A *strict common* key.  The `$` suffix enforces that all parent and child branches of the matched common node are marked as required, preventing alternative paths from being silently discarded. |

Internally, `SCKEY` tokens are handled like common keys whose transitions are created with the `CommonMode::ALL` flag, which is what triggers the stricter branch validation.  Regular `CKEY` transitions, by contrast, use the default `CommonMode::NONE`, allowing unmatched adjacent branches to be ignored when the structure of the graph makes that desirable.

## Generating example graphs

For reusable format-independent predicates such as `conv` across ONNX and
StableHLO, see [node and pattern libraries](doc/node-library.md).
Ready-to-run scripts and graph fixtures are in [matching examples](examples/matches/README.md).
The compiler's proposed [post-AST root selection phase](doc/post-ast-root-selection.md)
prepares traversal planning before TransitionGraph generation.

`PGInterpreterIR(query).interpret()` generates an example graph from a pattern.
An optional second constructor argument limits the randomly selected number of
copies for each `*`, and the additional copies after the mandatory occurrence of
each `+`. The default limit is 10. This is a per-quantifier limit, not a bound on
the total graph size, and does not change matching in `PGInterpreterTG`.

For deterministic zero-repetition cases, use `PGInterpreterIR(query, 0)`:
`A*` produces an empty graph, `A+` produces one A node, and `A->B*->C`
produces an A-to-C edge. The `?` quantifier remains random.

