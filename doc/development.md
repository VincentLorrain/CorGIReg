# Installation, verification and performance

## Installation

Use `python installation/install.py` for JSON support or add `--formats all`
for ONNX and StableHLO. The PowerShell and Bash scripts delegate to this shared
installer. If PowerShell blocks script execution, invoking the Python script
directly needs no execution-policy change. Full prerequisites, format selections,
custom directories and installed executable paths are in the
[installation guide](../installation/README.md).

The installer creates a local environment, compiles and installs the native
engine, runs discovered tests and verifies CLI examples. It does not install
system compilers. Python-only installation is insufficient without the worker.
See [CLI engine discovery](cli.md) when using a separate build.

## Functional tests

For an existing development toolchain, from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build -N
ctest --test-dir build --output-on-failure
```

Keep the generator/toolchain consistent with an existing build directory. On
Windows a new MinGW build can select `-G "MinGW Makefiles"`. Multi-configuration
generators additionally need `--config Debug` for building and `-C Debug` for
CTest. Wait for the build to finish before launching tests, particularly on
Windows where running executables can prevent linking.

CTest discovery must return a nonzero number of tests. Choose targeted `-R`
filters from `ctest -N`; a filter matching nothing proves nothing. Catch2 3.4.0
supplies `Catch2WithMain`, so test sources must not define another main. Test
`.cpp` files are discovered recursively in `test/CMakeLists.txt`.

Use explicit small graphs and expected results, with positive and negative
cases. Assert results, links or exceptions, rather than console output or
Mermaid exports. Use independent fixtures alongside generated graphs. Seed
random tests reproducibly and report failing inputs. Singleton predicate keys
must be unique to a test; Catch2 sections do not reset process-wide statics.

When Python is found, CTest includes CLI, installer and benchmark-report tests.
`CORGIREG_IMPORT_PYTHON` selects a Python interpreter with optional ONNX and
StableHLO dependencies for importer integration tests. Without it, those optional
checks are not evidence of importer coverage. The installer configures its
selected checks; see [test registration](../test/CMakeLists.txt).

## Performance history

Functional assertions check correctness. Timed benchmarks separately measure
pattern compilation, root planning, known-start traversal and multi-start search.
From the repository root:

```sh
python benchmarks/track.py run --output build/performance/reference.json
python benchmarks/track.py run --baseline build/performance/reference.json --output build/performance/current.json
```

Use a fresh filename for each run. The runner builds Release, warms up, samples
deterministic workloads and writes immutable JSON with raw timings, statistics,
Git commit/dirty-state fingerprints, machine and compiler metadata. A positive
percentage means slower, a negative percentage faster. Only compatible reports
can be compared; changes to fixtures or machines require a new reference.

The default 20% threshold is informational. `--fail-on-regression` enables a
nonzero regression status; it is not a statistical significance test. Timings
are not unit-test deadlines. Binary size is recorded, but runtime memory usage
is not measured. Dirty reports represent the working tree, not just the commit.

`CORGIREG_BUILD_BENCHMARKS=ON` enables the native benchmark target and its
correctness smoke test in CTest. The independent runner enables this in its own
build. Full workloads, JSON provenance, sampling options, comparison requirements
and exit codes are in the [benchmark reference](../benchmarks/README.md).

## CI and troubleshooting

The [main workflow](../.github/workflows/ci.yml) runs build/test checks;
[installation CI](../.github/workflows/installation.yml) exercises installer
paths; [performance CI](../.github/workflows/performance.yml) publishes timing
reports as artifacts. Hosted-runner timing variation is not an automatic
cross-commit performance verdict.

| Symptom | Check |
| --- | --- |
| `matches: []` | Operation names/aliases, missing nodes, plain-node degree constraints, boundaries and rule presence conditions. |
| Engine not found | Installed native worker, `--engine` or `CORGIREG_ENGINE`. |
| Unknown predicate | Raw op normalization and selected library/format mapping. |
| Unsupported IR | Importer subset, nested regions/graphs and function selection. |
| Start budget exceeded | Number of path starts; narrow the pattern or deliberately increase `--max-starts`. Root planning does not yet reduce enumeration. |
| Timeout | Ambiguity and graph size; `--timeout` bounds native matching, not Python import time. |
| Cannot write report | Parent directory exists and output differs from every input/definition file. |
| Dependency download fails | Network or local FetchContent dependency paths; distinguish setup failure from a test failure. |

For documentation-only changes, verify local links, JSON examples and executable
commands relevant to the edit. There is no need to claim a full recompilation
when no compiled source changed. Doxygen configuration exists in `doc/`, but is
not enabled by the root build; these Markdown guides are the maintained entry point.
