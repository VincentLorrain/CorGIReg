# Deterministic and seeded graph generation

`PGInterpreterIR` expands a pattern AST into a `GraphNN`. Pass
`IRGenerationOptions` or a values-file path to select the configured generator.
The parser and matching language are unchanged. This API generates topology;
it does not import a network or create ONNX tensors, shapes or weights.

## Explicit values

```cpp
#include "CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp"
using namespace CorGIReg;

IRGenerationOptions values;
values.repetitions = {{1, {3, 1}}, {2, {2}}, {3, {0}}, {4, {1}}};
auto graph = PGInterpreterIR("(A+->B)+->C*->D?", values).interpret();
// A -> A -> A -> B -> A -> B -> D
```

Quantifier indices start at **1**, in textual postfix order, just like
[matching traces](match-traces.md). In this example the inner `+` is 1,
the outer `+` is 2, `*` is 3 and `?` is 4. Indices identify AST occurrences,
not operator symbols: two distinct `+` operators have different indices.

| Symbol | Allowed explicit value |
| --- | --- |
| `*` | Zero or more occurrences |
| `+` | One or more occurrences, including the mandatory first occurrence |
| `?` | Exactly 0 or 1 |

A singleton list broadcasts its value to every invocation of that quantifier.
A longer list is consumed in expansion order and must have exactly one value
per invocation. Nested lists are flattened: `[3, 1]` gives the inner `+` three
occurrences during the first outer iteration and one during the second.
Each iteration expands the AST again; it does not clone an already sampled body.

Missing values are errors when the corresponding quantifier is reached.
An inner quantifier under a skipped outer group is not reached and needs no
value. Unknown indices, empty lists, illegal counts, exhausted lists and surplus
multi-value lists are errors. Supplied counts are validated even in skipped groups.
An unused singleton is allowed, which makes parameter sweeps convenient.

## Values files

```cpp
auto graph = PGInterpreterIR(
    "(A+->B)+->C*->D?", "examples/generation/nested.values").interpret();
```

The file is a small **text format**, not JSON or YAML:

```text
# Each numeric key is a quantifier index.
mode = deterministic
1 = 3, 1
2 = 2
3 = 0
4 = 1
```

Spaces, blank lines, CRLF and comments beginning with `#` are supported.
All numbers are unsigned decimal integers. Duplicate keys (including `1` and
`01`), unknown settings, trailing commas and malformed integers are rejected.
File syntax errors include the line number; pattern-dependent errors identify
the quantifier. Paths are relative to the caller's working directory.

`IRGenerationOptions::read(std::istream&)` supports in-memory configuration;
`IRGenerationOptions::fromFile(path)` loads options that callers can then adjust.
See the [complete example file](../examples/generation/nested.values).

## Random fallback

```text
mode = random
seed = 20261026
max_loops = 4
max_nodes = 100000
1 = 3
```

For `A+->B*->C?`, this always creates three A nodes. Missing `*` and `?`
values are sampled. Explicit lists never fall back to randomness when exhausted;
that is a configuration error.

| Setting | Default | Meaning |
| --- | --- | --- |
| `mode` / C++ `randomMissing` | `deterministic` / `false` | Error on missing reached counts, or sample them |
| `seed` | 5489 | Unsigned 32-bit seed for a local `mt19937` |
| `max_loops` / C++ `maxLoops` | 10 | Missing `*`: 0..limit; missing `+`: 1..limit+1 |
| `max_nodes` / C++ `maxNodes` | 100000 | Maximum number of distinct generated nodes |

Missing `?` values are sampled as 0 or 1 independently of `max_loops`.
Each call to `interpret()` resets the generator to its configured seed, so the
same options produce the same graph again. Increment the seed to generate a
series of random cases. Reproduction assumes the same standard-library random
distribution implementation; cross-toolchain identical sampling is not promised.

Counts and `max_loops` are limited to 1,000,000. Expansion also has a fixed
1,000,000 AST-visit budget, including empty bodies, to bound nested growth.
Exceeding the visit or node budget throws `std::length_error`; it never silently
truncates a graph. Randomly chosen counts can also exceed an expansion budget.

## Topology and identities

Sequence concatenates fragments in the arrow's direction; `;` combines paths.
Zero occurrences contribute an empty fragment, so `A->B*->C` with `*=0`
connects A directly to C. Plain nodes get distinct identities at each occurrence.
Common forms (`B#`, `$B`, `B$`, `$B$` with the same optional number) reuse one
node across paths and repetitions. Explicit edges between identical anchors
are retained as self-loops. Repetition containing a common node combines copies
around that anchor, following the matching machine's reference semantics.

Node metadata contains `Type`, `Key`, and `isCommon` for anchors. The configured
generator preserves insertion order in `GraphNN::getNodes()`, independently of
pointer addresses. It does not currently load per-node attributes or operation
templates from this values file; node predicate libraries remain separate.

Input ports can be fixed with `A-1->B` (or `B<-1-A`). `A-*->B` is an alias
of `A->B`: both allocate inputs in expansion order, starting at 1 independently
for each destination node. For example `(A->Sum#)+` with count 3 connects three
A nodes to ports 1, 2 and 3 of the shared Sum node. Generated output ports are
unspecified. Fixed and automatic inputs cannot be mixed on the same destination;
two distinct producers cannot occupy the same fixed input. A fixed numbered
connection requires nonempty endpoints; zero expansion of an entire endpoint
is an error. Ordinary arrows can still bridge empty fragments.

Matching interprets `->` and `-*->` identically as adjacency without a port
constraint; `-1->` requires an actual connection to input 1. Input arity and
operator semantics are not validated by this structural generator.

`$`, `#`, arrows and strict markers take no repetition value. A standalone `$`
adds no node. Strict and boundary constraints remain matching constraints: this
is a structural expander, **not a satisfiability solver**. Combining conflicting
paths or choosing a repetition count does not guarantee that the resulting
graph matches the entire query. Run the matcher when that guarantee is needed.

## Parameter series and testbench

```cpp
for (std::size_t count : {1u, 2u, 8u, 32u}) {
    IRGenerationOptions values;
    values.repetitions = {{1, {count}}, {2, {1}}};
    auto graph = PGInterpreterIR("A+->B?", values).interpret();
    // Supply graph to the testbench; expected size is count + 1.
}
```

The native benchmark uses configured generation to construct its 16- and
64-node matching fixtures outside the measured operation. Their edges are
checked against explicit expectations before matching. `generate_chain_64`
separately measures parsing, expansion, edge-count verification and cleanup.
See [performance tracking](../benchmarks/README.md). Changed benchmark sources
require a fresh reference fingerprint; do not compare incompatible suites.

Regression tests cover exact topology, reverse arrows, cycles, common anchors,
nested invocation values, series, file errors, seeded fallback and public
matching. Independent hand-built graph tests remain necessary: a generator
and matcher sharing the same AST must not be each other's only oracle.

## Existing constructors

`PGInterpreterIR(query)` and `PGInterpreterIR(query, maxLoops)` now use the same
expansion engine with random fallback and a randomly selected seed at construction.
Their repetition bounds are preserved, and the default node/visit limits apply.
Repeated interpretation of the same instance reuses its seed. Pass explicit
options or a values file to reproduce a fixture across independently created
instances. All constructors preserve port bindings and assign automatic inputs.

## Generation executable

Generation and matching are separate commands. From a built checkout:

```sh
./build/corgireg-generate -r '(A+->B)+->C*->D?' -v examples/generation/nested.values -o graph.json
./build/corgireg-generate -r '(A-*->Sum#)+' --random --seed 42 --max-loops 3 -o sum.json
corgireg -i graph.json -r 'A+-1->B'
```

On PowerShell, use `.\build\corgireg-generate.exe` with the same arguments.
`-r/--regex` is required. `-v/--values` loads the text configuration.
`-o/--output` selects a graph JSON file (default `-`, stdout). `--random`,
`--seed` and `--max-loops` override the corresponding file settings, independent
of option order. Defaults without a file are deterministic: missing reached
counts are errors. The executable generates and validates before opening its
output file. `--node-library PATH` (repeatable) and `--format NAME` emit library
keys as concrete operations; see [generating from library keys](node-library.md#generating-graphs-from-library-keys).
`--help` lists the options.

The output includes stable insertion-order node IDs, operation names and port
connections, and is directly accepted by the matching CLI. It exports topology,
not arbitrary `std::any` metadata. C++ consumers can use
`writeGraphJson(graph, stream)` from `GraphAdapters/GraphJson.hpp`.

The standard installation script installs both native commands into its
environment's `bin` directory. Neither command performs graph rewriting.
