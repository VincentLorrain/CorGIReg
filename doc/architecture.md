# Concepts and compiler architecture

CorGIReg searches an operation graph using a graph pattern. It preserves source
node identities and explains successful traversals, including quantifier values.
It does not execute a neural network, prove numerical equivalence, or rewrite
matched subgraphs.

## Representations

| Representation | Purpose | Reference |
| --- | --- | --- |
| Source IR | External ONNX, StableHLO or custom graph | [Importers](importers.md) |
| Operation graph | Normalized nodes, metadata and adjacency | [Graph JSON](graph-json.md) |
| Node definitions | Map semantic predicate names to source operations and conditions | [Library](node-library.md), [rules](node-rules.md) |
| Pattern text | Connectivity constraints and repetitions | [Language](pattern-language.md) |
| AST | Parsed pattern structure, with compact convergence expanded into paths | [Language](pattern-language.md#parsing-and-direction) |
| Root plan | Static analysis of which paths can share a start | [Root selection](post-ast-root-selection.md) |
| Transition graph / TGCompiled | Generated state machine and compiled executable matcher | [Adapters and C++ entry points](graph-adapters.md) |
| MatchSolution / JSON report | Captures and execution traces mapped to source IDs | [C++ traces](match-traces.md), [JSON report](match-report-json.md) |

The pattern IR and network IR are separate. An operation predicate such as `conv`
connects them through a node definition; it is not itself a subgraph pattern.
Patterns can be distributed as text files, and node definitions as JSON files.
There is no additional serialized AST format or named-pattern registry.

## Compilation

```text
pattern text -> PartitionGraphLexer -> PartitionGraphParser -> validated AST
    -> RootSelectionPass -> root plan
    -> PGInterpreterTG uses the AST and plan -> TransitionGraph -> TGCompiled

source file -> importer -> normalized graph -> ImportedGraph / GraphView
node definitions -> NodeLibrary (JSON, validation, op_types) -> compiled NodeTest rules -> predicates

TGCompiled + GraphView + predicates + starting tuple -> MatchSolution(s)
CLI search + source metadata -> public JSON report
```

The root pass produces plan-local indices, directed constraints, components,
and root candidates. The CLI uses `interpretRooted()`, which splits eligible
paths at their shared anchor and assigns those parts to a single start group.
Ineligible paths retain their traversal order. Quantified regions remain
opaque, and a quantified anchor disables rooting. Both machines compile the
same validated AST. See the [worked example](walkthrough.md#follow-the-compiler-and-the-search).

`PGInterpreterIR` is a separate AST visitor that generates example
`GraphNN` graphs. It accepts explicit quantifier counts and can fall back
to seeded choices within a configured bound.
It is not an external importer or the matching engine. Generated examples are
useful for exploration, but independent explicit fixtures are necessary for
correctness tests because both visitors share the parser.

### Pattern tokens and transitions

The lexer recognises three kinds of key tokens, each compiled into its own
transition type:

| Token | Lexical form | Transition type | Meaning |
| --- | --- | --- | --- |
| `KEY` | `A`, `foo_1` | `TransitionTypes::UNIQUE` | Node-condition key; the node must also have at most one parent and one child. |
| `CKEY` | `A#`, `foo#2` | `TransitionTypes::COMMON` | Common anchor shared by occurrences of the same identity; other neighbors stay free (`CommonMode::NONE`). |
| `SCKEY` | `$A`, `A$`, `$A$`, `$foo#3`, `$foo$3` | `TransitionTypes::STRICTCOMMON` | Common anchor whose inputs (`$A`, `INPUTS`), outputs (`A$`, `OUTPUTS`) or both (`$A$`, `ALL`) must be covered. |

Strictness refers to the source graph, independently of traversal direction,
and applies even to a single neighbor. The standalone boundary in `A->$` is a
separate `NNULL` transition. See [directional strict nodes](directional-strict-nodes.md).

An alternation compiles to an entry and an exit state joined to each
alternative by empty transitions that also pass at graph end. Alternatives
share their path origin: one alternative is taken per walk.

## Runtime responsibilities

`TransitionGraph` is a mutable compiler representation. `TGCompiled` holds
the machine used for search. Each transition owns its endpoint states.
States retain weak links to transitions so this ownership does not form a cycle.

A runtime context holds both a position in the machine and a position in the
source graph. Consuming transitions validate a node and move to its neighbors.
Internal transitions handle branching, quantified groups, port checks, and
end-of-walk assertions. A repetition can jump back to a candidate near a
bound anchor, then check the entire body against the same bindings. Trace
actions record the beginning, each iteration, the end, or a skip without
consuming an operation.

Each branch receives its own context. A strict occurrence records required
neighbors in that context. The matcher combines compatible accepting walks
and checks these requirements against the union of their captures. A boundary
assertion is checked earlier, against the current walk's available successors.

`GraphView` provides read-only reciprocal parent/child adjacency and optional
names. `NodePredicates` supplies callbacks by key. The graph must remain unchanged
while searched. `nodesAtDelta` follows an exact signed number of simple-path
steps; it is not a shortest-path distance metric. See [adapter contracts](graph-adapters.md).

`BasicGraphAdapter` bridges the existing NodeNN/GraphNN representation and retains
ownership. Its IDs are local to the view, not pointer addresses or globally
persistent operation IDs. `ImportedGraph` uses explicit imported IDs. Results
retain their graph view; C++ names are read from that view when requested.

Node libraries are explicit `NodeLibrary` objects owned by one invocation. The
C++ engine loads, validates and resolves them for both matching and generation;
Python front ends only import external formats and transport node documents.
Legacy NodeNN conditions use the process-wide `NodeDictionary` singleton. It
has no public reset; registration rejects duplicate/invalid keys. Imported
per-request predicates avoid changing that registry. Do not assume shared
compiler/global registry state is thread-safe without additional synchronization.

## Code map and extension points

| Directory | Responsibility |
| --- | --- |
| `include/BasicGraphIr`, `src/BasicGraphIr` | NodeNN and GraphNN representation |
| `include/GraphAccess`, `include/GraphAdapters` | GraphView contract, traversal helpers, and adapters |
| `include/CompileTool`, `src/CompileTool` | Tokens, AST, parser, interpreters and post-AST planning |
| `include/TransitionGraph`, `src/TransitionGraph` | Machine construction, execution and match traces |
| `include/NodeTest`, `src/NodeTest` | Predicate registry, legacy expressions, typed rules, JSON documents and node libraries |
| `include/Generation`, `src/Generation` | Seeded random pattern generator for tests and fuzzing |
| `include/Util` | Header-only combination and permutation iterators |
| `src/Cli`, `python/corgireg_cli` | Native worker and public CLI/importers |
| `test`, `benchmarks` | Functional regressions and measured workloads |

New input formats usually need an importer or adapter, not new state-machine
transitions. New semantic operations usually need a node definition, not new
syntax. Language changes need lexer/parser, interpreter and public-result tests;
runtime changes also need explicit positive and negative graphs. See
[development and performance](development.md).
