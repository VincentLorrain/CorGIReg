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
| AST | Parsed pattern structure retaining original occurrences | Compiler pipeline below |
| Root plan | Post-AST proposal for choosing traversal roots | [Root selection](post-ast-root-selection.md) |
| Transition graph / TGCompiled | Generated state machine and compiled executable matcher | [Adapters and C++ entry points](graph-adapters.md) |
| MatchSolution / JSON report | Captures and execution traces mapped to source IDs | [C++ traces](match-traces.md), [JSON report](match-report-json.md) |

The pattern IR and network IR are separate. An operation predicate such as `conv`
connects them through a node definition; it is not itself a subgraph pattern.
Patterns can be distributed as text files, and node definitions as JSON files.
There is no additional serialized AST format or named-pattern registry.

## Compilation

```text
pattern text -> PartitionGraphLexer -> PartitionGraphParser -> AST
    -> RootSelectionPass -> root proposal
    -> PGInterpreterTG visits original AST -> TransitionGraph -> TGCompiled

source file -> importer -> normalized graph -> ImportedGraph / GraphView
node definitions -> NodeLibrary (JSON, validation, op_types) -> compiled NodeTest rules -> predicates

TGCompiled + GraphView + predicates + starting tuple -> MatchSolution(s)
CLI search + source metadata -> public JSON report
```

The root pass produces plan-local node indices, directed constraints, components
and optional roots; `interpretRooted()` uses anchor roots to start each component
on its anchor, as the CLI does. Quantified regions remain opaque; quantified
common anchors trigger conservative fallback to path-order starts. Both machines
are generated from the original AST occurrences.

`PGInterpreterIR` is a separate AST visitor that **generates example GraphNN
graphs** using bounded random repetition choices (default repeat limit 10).
It is not an external importer or the matching engine. Generated examples are
useful for exploration, but independent explicit fixtures are necessary for
correctness tests because both visitors share the parser.

## Runtime responsibilities

`TransitionGraph` contains states and transitions; compilation prepares start
states and common-node constraints. Runtime contexts carry current source
position, common-anchor bindings and trace state. Transitions test predicates,
move through parents/children, enforce boundaries or perform internal reference
and empty transitions. Quantifier trace actions record begin, iteration, end
and skip events without consuming source nodes themselves.

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
| `include/`, `src/BasicGraph` | NodeNN and GraphNN representation |
| `include/`, `src/GraphAccess` | GraphView adapters and traversal helpers |
| `include/`, `src/CompileTool` | Tokens, AST, parser, interpreters and post-AST planning |
| `include/`, `src/TransitionGraph` | Machine construction, execution and match traces |
| `include/`, `src/NodeTest` | Predicate registry, legacy expressions, typed rules, JSON documents and node libraries |
| `src/Cli`, `python/corgireg_cli` | Native worker and public CLI/importers |
| `test`, `benchmarks` | Functional regressions and measured workloads |

New input formats usually need an importer or adapter, not new state-machine
transitions. New semantic operations usually need a node definition, not new
syntax. Language changes need lexer/parser, interpreter and public-result tests;
runtime changes also need explicit positive and negative graphs. See
[development and performance](development.md).
