# Explaining a match

`MatchSolution::getQuery()` returns the complete input pattern, including its
quantifiers. `getAll()` and `at(predicate)` continue to return source node IDs.
`getWalks()` additionally exposes the successful execution that produced those
captures, without sorting nodes by their numeric ID.

For `A+->B`, matching source nodes `42:A -> 0:A -> 9:B` gives:

- Query: `A+->B`.
- Ordered captures: `(42, A), (0, A), (9, B)`.
- Quantifier index `1`, symbol `+`, count `2`.
- Quantifier capture range `[0, 2)`, iteration ends `[1, 2]`.

These C++ fields are also exposed by the CLI's [JSON match report](match-report-json.md).
The JSON names use `id`, `count` and `iteration_ends` for the C++ capture's
`node`, `count()` and `iterationEnds` respectively.

## Source names and identity

Adapters can override the optional `GraphView::nodeName(NodeId)` method to return
an IR operation name. Its default is the decimal ID, so existing adapters still
work. `match.getNodeName(id)` delegates to that method. Names need not be unique;
always retain the ID and graph view to identify the source operation exactly.
The result keeps the view alive. Names are read from the view when requested,
not copied into a snapshot.

## Walks and quantifier executions

Each `MatchWalk` contains:

| Field | Meaning |
| --- | --- |
| `origins` | Automaton origins covered by this walk; internal IDs, not source positions. |
| `nodes` | Captures in execution order, each with `node` and `predicate`. |
| `quantifiers` | Quantifier invocations in execution order, including skipped invocations. |

Independent branches of a graph have separate walks. There is no total order
between those walks. Common nodes can occur several times in a walk, while
`getAll()` contains each node only once.

Each `QuantifierCapture` contains:

| Field | Meaning |
| --- | --- |
| `index` | One-based index of the quantifier symbol in the original pattern, left to right. |
| `symbol` | `+`, `*` or `?`. |
| `count()` | Number of completed body iterations for this invocation. |
| `parent` | Optional zero-based index of the enclosing invocation in this walk's `quantifiers`. |
| `begin`, `end` | Half-open range in this walk's `nodes`. |
| `iterationEnds` | Exclusive capture end of each completed iteration. |

The first iteration starts at `begin`; subsequent ones start at the preceding
iteration end. Counts refer to whole repeated bodies, not distinct nodes.
`(A->B)+` over `A->B->A->B` has count `2`, not `4`.

For `(A+->B)+` over `A->A->B->A->B`, the textual indices are `1` for
the inner `+` and `2` for the outer `+`. The invocation vector is:

| Vector position | Quantifier index | Count | Parent position | Capture range |
| --- | --- | --- | --- | --- |
| 0 | 2 | 2 | absent | `[0, 5)` |
| 1 | 1 | 2 | 0 | `[0, 2)` |
| 2 | 1 | 1 | 0 | `[3, 4)` |

Use `index` to group values in textual order; use vector order and `parent` to
preserve execution and nesting. This distinction also applies to `<-` patterns.
A skipped outer group records count zero; its inner quantifiers have no invocation.

## Reading the result

```cpp
#include "TransitionGraph/MatchSolution.hpp"
#include <ostream>

void describeMatch(const CorGIReg::MatchSolution& match, std::ostream& output) {
    output << "Pattern: " << match.getQuery() << '\n';
    for (const auto& walk : match.getWalks()) {
        output << "Walk:\n";
        for (const auto& capture : walk.nodes) {
            output << "  " << capture.node << " " << match.getNodeName(capture.node)
                   << " matched " << capture.predicate << '\n';
        }
        for (const auto& quantifier : walk.quantifiers) {
            output << "  Quantifier " << quantifier.index << " " << quantifier.symbol
                   << " = " << quantifier.count() << '\n';
        }
    }
}
```

## Ambiguity and termination

Different allocations remain separate matches: `A+->A+->$` over three `A` nodes
returns counts `(1, 2)` and `(2, 1)`, even though both capture the same node set.
Result ordering is not a ranking or a greedy-match guarantee. Preserving more
alternatives increases memory use and runtime for ambiguous patterns.

The same iterations taken in another order are not another match. In
`(B->D#)+` around one `D`, the set of branches {B1, B2} is reported once, not
once as B1 then B2 and again as B2 then B1. The search explores a walk once
per state, position, captures, anchors and completed iterations (compared as a
set), so every distinct count, split and grouping of nodes into iterations is
still reported. The kept trace lists iterations in the order the search first
found them. Walks that end at different positions but report the same trace
also give one match.

Zero occurrences are supported at graph end. To avoid infinitely many traces
for nullable or common-node-only cycles, the first body iteration may add no
new node, but each additional iteration must cover at least one new source node.
For example, `(A?)*` when `A` rejects produces canonical outer counts zero and
one; it does not enumerate arbitrarily many empty iterations.

Quantifier metadata is generated by `PGInterpreterTG`. Hand-built automata retain
their query and node captures but do not infer quantifiers from query text or
cycles. Unannotated cycles retain the existing runtime-context deduplication.
These APIs describe successful node visits; they are not an induced-subgraph
export or a list of all edges between captured nodes.

Regression coverage is in `test/App/match_trace_test.cpp`, including zero, optional,
nested and ambiguous repetitions, reverse syntax, common branches, interpreter
reuse, names, source ID zero and termination of unannotated cycles.
