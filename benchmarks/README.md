# Performance history by commit

From the repository root, on Windows or Linux:

```sh
python benchmarks/track.py run
```

Use `python3` on Linux if that is the Python command on your PATH. Python 3.10+,
Git, CMake and a C++17 toolchain are required; no Python packages or model readers
are needed. The first build may download fmt. The script configures a separate
Release build in `build/performance`, rebuilds the benchmark, then writes a new
JSON file under `build/performance/reports/`, named with a UTC timestamp and Git
revision. It never overwrites an existing report.

## Establish and compare a reference

```sh
python benchmarks/track.py run --output build/performance/reference.json
```

After a later change or commit, on the same machine:

```sh
python benchmarks/track.py run --baseline build/performance/reference.json --output build/performance/current.json
```

The command prints the median time per operation and percentage change. Positive
percentages mean **slower**; negative percentages mean **faster**. A difference
within 20% is classified `within_threshold` by default. This threshold is a
configurable signal, not a statistical significance test.

Compare existing files without compiling:

```sh
python benchmarks/track.py compare build/performance/reference.json build/performance/current.json --threshold 15 --output build/performance/comparison.json
```

Add `--fail-on-regression` to return exit code 2 when a median exceeds the
slowdown threshold. Exit code 1 indicates an error or incompatible reports.
The default is informational, so a noisy measurement does not fail functional
tests. `run --fail-on-regression` requires `--baseline`.

Choose new output filenames for each measurement, or omit `--output` for automatic
history filenames. References are ordinary saved reports: explicitly select
which one to compare against. Local reports are ignored by Git under `build/`.
If a reference needs to be versioned, copy the selected JSON to a tracked path
and commit it intentionally; do not update it automatically after each run.

## Measured workloads

| Case | Timed work | Expected result |
| --- | --- | --- |
| compile_shared | Parse and compile `A+->B#;B#->C#;C#->D`, including post-AST planning | Nonempty machine |
| post_ast_shared | RootSelectionPass on an already parsed AST | One component rooted at B# |
| match_chain_16 / match_chain_64 | Execute an already compiled `A+->$` from a known start | Full chain, repetition count 16 / 64 |
| search_shared_6 / 12 / 48 | Native request processing, compilation, multi-start search and JSON serialization | 2 / 4 / 16 distinct three-node groups |
| search_shared_48_jobs2 / jobs4 | Same 48-node search using 2 / 4 workers | 16 distinct three-node groups |
| search_shared_192 / _jobs2 / _jobs4 | Same 192-node search using 1 / 2 / 4 workers | 64 distinct three-node groups |

Parallel scenarios include pool startup and ordered-result coordination. Compare
their medians against the matching sequential workload within the same report.
The runner's `--jobs` option controls build parallelism, not these fixed worker
counts. Adding these scenarios changes the suite fingerprint and requires a new
reference. See [the multithreading contract](../doc/multithreading.md).

Fixtures are explicit and deterministic; they do not use PGInterpreterIR or
random graph generation. Fixture construction is outside timing. Each scenario
verifies its result, including chain captures and quantifier counts. Several
execution traces may describe the same group, so search checks distinct node
groups rather than requiring one serialized trace per group. Verification and
result destruction inside the operation are part of the measured workload.

The multi-start cases use `A->B#;B#->C` over disjoint A-B-C chains. Their logical
start arity is 2. They are intended to track the benefit of future rooted search
against the current Cartesian enumeration. They do not imply that the proposed
post-AST plan has already changed execution.

## Measurement and provenance

The C++ process uses `steady_clock`, with three warmup operations followed by
seven samples of three operations each by default. A sample is the batch duration
divided by the number of operations. Process startup, Python, CMake and compilation
of the executable are excluded. The `compile_shared` case intentionally measures
runtime compilation of the pattern itself.

Reports contain raw nanoseconds per operation, median, minimum, maximum, nearest-rank
p95 and median absolute deviation (MAD), plus:

- Git revision, dirty state and a fingerprint of tracked changes/untracked files.
- UTC timestamp, machine identity, OS, CPU description and logical CPU count.
- Compiler, build configuration, CMake compilation/link flags and generator.
- Benchmark-suite fingerprint, sample configuration and executable hash/size.
- Input size, logical start arity and expected result for each case.

The runner rebuilds before measuring and refuses to save a report if the source
state changes during the run. A dirty report describes that working-tree state,
**not** the clean commit alone. Existing local edits are retained.

Comparisons require matching machine, build metadata, sampling settings, suite
fingerprint and workload definitions. Different source commits are expected.
Changing benchmark scenarios requires establishing a new reference. The binary
size is recorded as metadata; it is not a measurement of runtime memory use.

For more samples, use `--samples 21 --iterations 10` on both reference and current
runs. Run on an otherwise idle machine with consistent power settings. Inspect
dispersion and repeat measurements before concluding that a code change caused
a regression. The script does not pin CPUs or control background processes.

`--config`, `--build-dir`, `--generator` and `--jobs` select the build. Avoid
reconfiguring your functional Debug build; the dedicated default directory keeps
the two separate. `--machine` provides a stable name only when measurements
really use the same physical machine; it should not be used to mask hardware changes.

## Tests and CI

Report calculations and compatibility checks are part of the regular Python
CTest tests. To also run native benchmark correctness as a smoke test:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCORGIREG_BUILD_BENCHMARKS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The smoke test checks correctness, not a speed threshold. It is a separate
executable, so Catch2WithMain keeps its single existing main.

The `Performance reports` GitHub Actions workflow measures Release builds on
pushes and pull requests and uploads JSON artifacts labelled with the commit,
retained for 90 days. Hosted runners vary: these artifacts record history but
do not automatically gate speed changes or form a reliable cross-machine baseline.
Use a stable machine or dedicated runner for commit-to-commit speed decisions.
