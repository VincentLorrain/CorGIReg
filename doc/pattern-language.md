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
| `B$` | Strict common node: additionally requires adjacent source nodes to be covered by the solution. |
| `A->$` | Boundary assertion: no following source node. The standalone `$` consumes no operation. |
| `A->B#; B#->C` | Two paths joined through their common B anchor. |

Predicate keys contain ASCII letters, digits and underscores. They resolve to
raw operation predicates or [node-library definitions](node-library.md).
`B#1` and `B$1` share identity, while retaining their respective constraints.
Numeric suffixes are interpreted numerically (`#01` and `#1`); an omitted suffix
differs from explicit zero. Plain repeated B occurrences do not establish shared
identity. Keep suffixes within the implementation's integer range.

There is no alternation `|`, wildcard `.`, character class, `{m,n}`, lazy or
possessive quantifier. Attribute regexes belong to [NodeTest rules](node-rules.md),
whose ECMAScript string syntax is separate from this graph language.
`(A->B)` and `B#+` are invalid; `(B#)+` is the quantified-group spelling.

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
The CLI currently enumerates starting tuples with a `--max-starts` budget
(default 100000); exceeding it is an error, not a partial successful report.
The [post-AST root proposal](post-ast-root-selection.md) does not yet replace
this enumeration.

Quantifier results count whole-body iterations. Ambiguous allocations remain
distinct solutions and ordering is not a greedy ranking. Nullable and repeated
common-node patterns use finite canonical traces: after the first iteration,
each additional iteration must cover a new source node. See
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
