# CorGIReg documentation

Start with [concepts and architecture](architecture.md), then use the reference
for the representation or API you work with. Commands below assume the repository
root as the working directory.

## Install and run

- [Windows/Linux installation](../installation/README.md): prerequisites, local environments, scripts and verification.
- [CLI](cli.md): flags, engine selection, input/output files and examples.
- [Runnable matching scripts](../examples/matches/README.md): basic and conditional matches.
- [Example library catalog](../examples/library/README.md): reusable operation definitions and patterns.

## Data formats and semantics

- [Operation graph JSON](graph-json.md): every input field, IDs, edges, normalization and metadata.
- [ONNX and StableHLO importers](importers.md): supported subsets, source names and attribute conversion.
- [Pattern language](pattern-language.md): arrows, common/strict nodes, quantifiers, boundaries and parsing.
- [Node-definition JSON](node-library.md): format mappings, schema versions, alias resolution and loading.
- [NodeTest rules](node-rules.md): arguments, presence/absence, values, per-key regexes and logical conditions.
- [Match-report JSON](match-report-json.md): complete output fields, captures, source identities and quantifier invocations.
- [Match traces](match-traces.md): C++ result API, nested quantifiers, ambiguity and termination.

## Implementation and maintenance

- [Architecture](architecture.md): representations, compiler/runtime phases and source-code map.
- [Graph adapters](graph-adapters.md): minimal C++ topology interface and matching entry points.
- [Post-AST root selection](post-ast-root-selection.md): current proposal, selection rules and execution limits.
- [Multithreading experiment](multithreading.md): worker isolation, ordered results, `$` regressions and performance comparisons.
- [Native protocol](native-protocol.md): Python-to-worker transport versions and typed values.
- [Development and performance](development.md): tests, benchmark references, CI and troubleshooting.
- [Performance report details](../benchmarks/README.md): measurements, JSON provenance and commit comparisons.

## Terminology

An **operation** is a node of the source graph. A **predicate** classifies that
operation. A **pattern** combines predicates with graph constraints. An **anchor**
is a shared common-node identity within a pattern. An **AST occurrence** is a
position in the parsed pattern, not a source operation. A **state** belongs to
the traversal machine. A **capture** records a successful source-node visit.
A **walk** orders captures from one execution path. A **solution** combines
successful walks; its unique node set can coincide with another solution's set
while its quantifier allocations differ.
