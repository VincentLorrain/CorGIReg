# CorGIReg - Core Graph Intersect Regex

[![CorGIReg CI](https://github.com/VincentLorrain/CorGIReg/actions/workflows/ci.yml/badge.svg)](https://github.com/VincentLorrain/CorGIReg/actions/workflows/ci.yml)

CorGIReg is an C++ library designed to apply regular expression operations on graph structures. This library extends traditional text-based regular expressions to graphs, allowing for powerful and flexible pattern matching in complex data structures.

To run patterns directly on an existing graph through a minimal read-only
interface, see [Graph adapters](doc/graph-adapters.md). Results expose graph-local
node IDs, with an adapter for resolving existing `NodeNN` objects.

# Graph-Regex Token Types

CorGIReg extends conventional regular-expression syntax with tokens that describe how a node condition should be interpreted inside a transition graph.  The lexer recognises three flavours of "key" tokens, each mapping to a distinct transition type:

| Token | Lexical form | Transition type | Meaning |
|-------|--------------|-----------------|---------|
| `KEY` | `A`, `foo_1` | `TransitionTypes::UNIQUE` | A standard node-condition key; the transition succeeds when the node identified by the key matches. |
| `CKEY` | `A#`, `foo#2` | `TransitionTypes::COMMON` | A *common* key.  The `#` suffix groups multiple transitions that should share the same common anchor, but without enforcing that every adjacent branch of the common node must be consumed. |
| `SCKEY` | `A$`, `foo$3` | `TransitionTypes::STRICTCOMMON` | A *strict common* key.  The `$` suffix enforces that all parent and child branches of the matched common node are marked as required, preventing alternative paths from being silently discarded. |

Internally, `SCKEY` tokens are handled like common keys whose transitions are created with the `CommonMode::ALL` flag, which is what triggers the stricter branch validation.  Regular `CKEY` transitions, by contrast, use the default `CommonMode::NONE`, allowing unmatched adjacent branches to be ignored when the structure of the graph makes that desirable.

## Generating example graphs

`PGInterpreterIR(query).interpret()` generates an example graph from a pattern.
An optional second constructor argument limits the randomly selected number of
copies for each `*`, and the additional copies after the mandatory occurrence of
each `+`. The default limit is 10. This is a per-quantifier limit, not a bound on
the total graph size, and does not change matching in `PGInterpreterTG`.

For deterministic zero-repetition cases, use `PGInterpreterIR(query, 0)`:
`A*` produces an empty graph, `A+` produces one A node, and `A->B*->C`
produces an A-to-C edge. The `?` quantifier remains random.

