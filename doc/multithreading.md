# Parallel starting-tuple search

The `multithreading` branch experiments with parallel execution of independent
starting tuples. The default remains one worker:

```sh
corgireg -i doc/examples/graph.json -r 'A+->B->$' --jobs 4 -o matches.json
```

`-j` is the short spelling of `--jobs`; accepted values are 1 through 256.
The actual worker count cannot exceed the number of tuples. One worker executes
directly on the calling thread, with no pool. Select a count suited to your CPU;
more threads do not guarantee faster matching.

## What runs in parallel

Import, pattern parsing/compilation, post-AST planning and node-rule filtering
complete before worker creation. Workers share one compiled machine, the source
graph and read-only predicate memberships. Each job executes a complete
`TGCompiled::test` tuple and serializes its matches into a local fragment.

Runtime environments, current positions, common bindings, strict obligations,
captures and quantifier invocations belong to that tuple. Branches of a tuple
still execute together, preserving common-node consistency. The compiler's
static state counter and common-key registry are not accessed by worker traversal.
No machine is recompiled or copied per tuple.

`Cli/SearchExecutor` creates a fixed worker pool for the request. A condition
variable bounds the dispatch window to at most eight tuples per worker. Results
are consumed on the calling thread in tuple-index order, regardless of completion
order. Only the caller writes the output stream. This bounds buffered **tuple
results**, not bytes: one highly ambiguous tuple can still produce a large result.
There is no new global match-memory limit.

Worker exceptions cancel undispatched work, wake waiting threads, join all
workers and propagate to the caller. Already running tuples finish before joining;
there is no cancellation inside a traversal. Output-consumer and thread-creation
failures also join the workers already created. The Python timeout still bounds
the native subprocess, and failed requests do not replace an existing report.

## Semantics and limits

For the same compiled machine, graph and starting sequence, parallel execution
preserves sequential tuple and match order, duplicate solutions, source names,
walks, origins and quantifier captures. Separate compilations can still order
start states differently; this experiment does not change that existing behavior.
The public report schema and wire protocol versions remain unchanged. The native
worker accepts `--jobs N` as a process argument; C++ callers can pass a third
`jobs` argument to `matchRequest(input, output, jobs)`.

This is not a promise that all library operations are thread-safe. Concurrent
compilation, mutation through exposed state/transition pointers, legacy registry
updates and user callbacks with mutable shared state remain outside the contract.
Custom GraphView implementations and predicates must support concurrent reads
before using them with a parallel executor. Do not parallelize individual branches
that share one runtime context.

The Cartesian starting-tuple count and `--max-starts` check are unchanged.
Root selection is still a planning pass. Parallelism distributes existing work;
it does not remove combinatorial growth.

## Verification and measurements

`test/App/parallel_matching_test.cpp` compares complete results from one compiled
machine with one and four workers, repeatedly. Fixtures include common/strict
branches, rejected strict matches, nested/ambiguous repetitions, reverse syntax,
source ID zero, empty graphs and common cycles. Both meanings of `$` are covered:
directional strict `$PB`, `PB$`, `$PB$` and boundary `->$`, including nullable tails and reversed boundaries.
Tests assert expected positive/negative outcomes independently of the comparison.

Executor tests check actual overlapping workers, ordered delivery across window
reuse, worker limits and exception propagation. Python CLI tests compare parsed
reports with 1/2/4/16 workers, including typed node rules and output preservation.

```sh
ctest --test-dir build -R 'Ordered search|Parallel tuple|CLI_Python' --output-on-failure
python benchmarks/track.py run
```

The benchmark now compares the same 48-node and 192-node shared-anchor searches
at 1, 2 and 4 workers. Names without `_jobsN` use one worker. Timings include
pool creation, compilation, matching, serialization and correctness checks;
they exclude Python process startup and network import. Compare medians within
one report, then repeat runs to check variability. See the
[benchmark guide](../benchmarks/README.md) for report provenance.

The suite fingerprint changes when these workloads are added: create a new
reference instead of comparing against a report from the previous suite.
The benchmark runner's own `--jobs` option controls **build parallelism**, not
matching workers; matching counts are fixed in the named scenarios.

ThreadSanitizer is an additional race-detection check, separate from functional
equivalence. On a supported Linux GCC/Clang environment, a dedicated Debug build
can use `-fsanitize=thread -fno-omit-frame-pointer` for C++ flags and
`-fsanitize=thread` for executable linker flags, then run the parallel tests.
The Windows validation of this prototype does not constitute a ThreadSanitizer
run, and performance numbers should never be measured under sanitizers.

## Initial local measurements

Two runs on 2026-10-23, Windows 11, GNU 12.2.0/MinGW, Release `-O3 -DNDEBUG`,
16 logical CPUs, three warmups and seven samples of three operations gave:

| Workload | 1 worker, median ms | 2 workers, median ms | 4 workers, median ms | 4-worker speedup |
| --- | --- | --- | --- | --- |
| 48 nodes, run 1 | 12.78 | 12.51 | 8.65 | 1.48x |
| 48 nodes, run 2 | 12.99 | 11.80 | 8.77 | 1.48x |
| 192 nodes, run 1 | 188.19 | 160.73 | 94.38 | 1.99x |
| 192 nodes, run 2 | 184.71 | 150.26 | 90.33 | 2.04x |

These are synthetic shared-anchor searches, not ONNX/StableHLO workload claims.
The local reports are `build/performance/reports/multithreading-first.json` and
`multithreading-repeat.json`. They identify commit `c5d6254` plus a dirty-tree
fingerprint (added benchmarks/documentation and pre-existing local edits).
Reports are generated artifacts rather than automatically versioned baselines.
Four workers helped these cases, while two workers gave only a modest gain on
the smaller case. Keep sequential execution as the default and measure actual
patterns before selecting a worker count.
