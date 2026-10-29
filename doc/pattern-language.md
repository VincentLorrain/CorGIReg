# Graph pattern language

A pattern describes operation predicates and connectivity, not characters in
a string. Pass one complete pattern with `-r`, or store it as UTF-8 text and
use `-R`. A file can contain semicolon-separated paths and whitespace; it is
not a list of separately executed queries. Comments are not supported.

## Tokens and semantics

| Syntax | Meaning |
| --- | --- |
| `A` | Predicate named A, with at most one parent and one child in the source graph. |
| `A->B` | A followed by B along a producer-to-consumer edge. |
| `B<-A` | Reverse spelling of A followed by B. |
| `A+` | One or more occurrences. |
| `A*` | Zero or more occurrences. |
| `A?` | Zero or one occurrence. |
| `(A->B)+` | Repeat the entire A-B sequence. Parenthesized groups require a postfix quantifier. |
| `B#` | Common node: occurrences of this anchor must identify the same source node. Allows branching. |
| `B#1`, `B#2` | Separate common identities for the same predicate B. |
| `$B` | Common node whose input neighbors must all be covered. Outputs remain free. |
| `B$` | Common node whose output neighbors must all be covered. Inputs remain free. |
| `$B$` | Common node requiring coverage of both input and output neighbors. |
| `A->$` | Boundary assertion: no following source node. The standalone `$` consumes no operation. |
| `A->B#; B#->C` | Two paths joined through their common B anchor. |

Predicate keys contain ASCII letters, digits and underscores. They resolve to
raw operation predicates or [node-library definitions](node-library.md).
`B#1`, `$B#1`, `B$1` and `$B$1` share identity, while retaining their respective
constraints. `$B#1` is the numbered input-only form; `$B1` names predicate B1.
Numeric suffixes are interpreted numerically (`#01` and `#1`); an omitted suffix
differs from explicit zero. A source node holds at most one identity: `B#1` and
`B#2` bind different nodes, and a walk reaching a bound node as another identity
is rejected. A plain occurrence is a node of its own: no other occurrence of the
match, plain or anchor, in the same or another path, captures it. `A->B;B#->C`
therefore needs `A->B#;B#->C` to share B. Each combination of path walks is
reported once. Plain repeated B occurrences do not establish shared
identity. Suffix identities are normalized digit strings, without integer conversion.

There is no alternation `|`, wildcard `.`, character class, `{m,n}`, lazy or
possessive quantifier. Attribute regexes belong to [NodeTest rules](node-rules.md),
whose ECMAScript string syntax is separate from this graph language.
`(A->B)`, `B#+` and `$B+` are invalid; use `(B#)+` or `($B)+`.
Markers must touch their predicate: `$ B` is not `$B`.

These strict markers require neighbor coverage, not a particular degree or
neighbor type. Constraints apply even at degree one and do not change meaning
under `<-`. The standalone `$` still asserts a boundary. See
[directional semantics and migration from V0](directional-strict-nodes.md).

## Parsing and direction

The following grammar summarizes the current parser (KEY excludes common suffixes):

```text
query      := seq (';' query | ';')?
seq        := exp ('->' (seq | '$') | '<-' rseq)? | '$' '<-' rseq
rseq       := exp ('<-' (rseq | '$'))?
exp        := KEY quantifier? | COMMON | STRICT_COMMON | '(' seq ')' quantifier
quantifier := '+' | '*' | '?'
```

Arrow sequences associate to the right. The reverse-arrow interpreter places
the entire right subtree before the left operand. Do not treat mixed arrows as
an arbitrary undirected drawing: `A->B<-C` follows those parser rules, and
`A<-B->C` is rejected. For branching, use explicit paths sharing common anchors,
such as `A->B#; C->B#`. This also makes the intended identity constraints visible.

## Matching behavior

The CLI searches candidate starts throughout the graph, so `A+->B` can return
both a complete A-chain and its suffixes. Matches need not consume the whole
source graph. A boundary assertion constrains that path's end; it does not
require coverage of all disconnected nodes. Plain predicates reject source
branching even when the operation type matches; use common-node syntax where
the pattern needs a branch or join.

Semicolons combine path constraints, not alternatives. Paths without shared
anchors remain disconnected constraints and can require combinations of starts.
The CLI enumerates starting tuples over the nodes each start can accept first,
with a `--max-starts` budget (default 100000); exceeding it is an error, not a
partial successful report.
The [post-AST root proposal](post-ast-root-selection.md) does not yet replace
this enumeration.

Quantifier results count whole-body iterations. Ambiguous allocations remain
distinct solutions and ordering is not a greedy ranking. Nullable and repeated
common-node patterns use finite canonical traces: after the first iteration,
each additional iteration must cover a new source node. In a repeated group
containing an anchor, each iteration reaches the same anchor node and may start
at any distance from it allowed by the body: `(A?->D#)+` and `(A+->D#)+` accept
branches of different lengths joining at D. See
[match traces](match-traces.md) for nesting, zero counts and termination.

## Practical examples

Against [the documentation graph](examples/graph.json):

| Pattern | Expected behavior |
| --- | --- |
| `A+->B` | Two solutions: A-A-B and A-B. |
| `A+->B->$` | Same solutions; B has no child. |
| `B<-A` | One A-B pair, starting from the A adjacent to B. |
| `A+->B#;B#->C` | No match because no C exists. |

For reusable semantic patterns and conditional operations, start with
[the node library](node-library.md) and [runnable scripts](../examples/matches/README.md).
# Numbered input arrows

`A-1->B` constrains the connection to input 1 of B. `B<-1-A` is its reverse
textual form. Ports start at 1 and must fit unsigned 32 bits. The plain arrows
`->` and `<-` remain valid; `-*->` and `<-*-` are their explicit wildcard aliases.
Whitespace can surround an arrow, but cannot split its internal spelling.
Numeric predicate names remain legal, for example `A->123`.

Matching fixed arrows requires port metadata in the source graph; an unspecified
port does not match a fixed number. Wildcard arrows do not constrain ports.
Quantifiers retain their existing meaning. A fixed connection requires a node
on both sides: skipping an entire operand cannot produce a numbered-edge match.
Ports label connections, not common-node identities or node IDs. Output ports
are preserved in the graph but are not selected by pattern syntax in this version.

Generation assigns sequential input ports for wildcard arrows and preserves
fixed numbers. See [generation](ir-generation.md) and [graph JSON](graph-json.md).
