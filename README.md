# CorGIReg - Core Graph Intersect Regex

[![CorGIReg CI](https://github.com/VincentLorrain/CorGIReg/actions/workflows/ci.yml/badge.svg)](https://github.com/VincentLorrain/CorGIReg/actions/workflows/ci.yml)

CorGIReg is a C++ library designed to apply regular expression operations on graph structures. This library extends traditional text-based regular expressions to graphs, allowing for powerful and flexible pattern matching in complex data structures.

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

## Pattern language at a glance

```text
A->B            A followed by B along a producer-to-consumer edge
A+->B           one or more A, then B
(A->B)*         repeat the whole A-B sequence zero or more times
A#->B;C->A#     two paths joined through the same source node A
$A$             common node whose inputs and outputs must all be covered
A->$            A has no successor
```

See the [pattern language](doc/pattern-language.md) for the complete syntax and
its semantics, and [directional strict nodes](doc/directional-strict-nodes.md)
for the migration from V0 (`A$` became `$A$`).

## More

- [C++ SDK and integration test](doc/library-integration.md) for use from a separate graph engine.
- [Node and pattern libraries](doc/node-library.md) for format-independent predicates such as `conv`.
- [Matching examples](examples/matches/README.md) with ready-to-run scripts and graph fixtures.
- [IR generation](doc/ir-generation.md): `corgireg-generate` and `PGInterpreterIR` build example graphs from a pattern.
- [Architecture](doc/architecture.md), including the [post-AST root selection phase](doc/post-ast-root-selection.md).
