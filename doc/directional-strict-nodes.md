# Directional strict nodes (after V0)

The position of `$` specifies which side of a common source node must be covered
by a successful solution. It does not specify how many connections exist.

| Pattern key | Input neighbors | Output neighbors |
| --- | --- | --- |
| `B#` | Additional neighbors allowed | Additional neighbors allowed |
| `$B` | All must be captured | Additional neighbors allowed |
| `B$` | Additional neighbors allowed | All must be captured |
| `$B$` | All must be captured | All must be captured |

Inputs are the source graph's parents; outputs are its children. These meanings
are absolute, including with reverse-arrow syntax or a parent-directed native
transition. Coverage refers to source-node IDs in the combined successful match,
not just the text immediately next to B or one branch's walk. Self-loops refer to
the same source node and are covered when that node is captured.

Zero neighbors impose no requirement. One neighbor imposes a real requirement:
`B$` by itself cannot match a B that has one uncaptured child. Several edges to
the same operation remain one neighbor in GraphView; this is not a port-count
constraint or a declaration that all neighbors have the same operation type.

## Examples

Given `A -> B -> C`, with an extra `X -> B`:

- `A->B#->C` matches.
- `A->$B->C` fails: input X is not captured.
- `A->B$->C` matches: C covers the complete output side.
- `A->$B$->C` fails: input X is not captured.

If the extra edge is instead `B -> X`, input-only strictness matches and
output-only strictness fails. `C<-$B<-A` has the same input requirement as
`A->$B->C`; changing the spelling's arrow direction does not swap the constraint.

For two input branches and one output, write:

```text
A->$B; X->$B; B#->C
```

The shared B identity joins the three paths. Additional B outputs remain allowed.
Use `$B$` to require both sides. Multi-path searches still have the usual
starting-tuple budget; these markers do not themselves optimize the search.

## Identity, numbering and repetition

`B#`, `$B`, `B$` and `$B$` refer to the same unnumbered anchor. Numbered spellings
are `B#1`, `$B#1`, `B$1` and `$B$1`. Prefix-only numbering uses `#` to separate
the predicate from the number: `$B1` means the predicate **B1**, not anchor 1 of B.
Leading zeros normalize (`#01` equals `#1`); omitted suffix and explicit zero are
distinct. Markers must be adjacent to the predicate. `$B#` is also an unnumbered
input-strict spelling, though `$B` is shorter.

Strictness belongs to each occurrence, while identity is shared. Different strict
occurrences contribute their required neighbors to the resulting solution.
The AST preserves the original spellings; both root planning and generated IR
normalize only anchor identity. Capture predicates remain `B`, without markers,
and the report's complete `query` retains the original syntax.

Quantify a common node through a group: `($B)+`, `(B$)*`, `($B$)?`.
A skipped group has count zero and imposes no constraints from its unvisited
body. An unsuccessful alternative must not impose its obligations on a successful
walk. Once traversed, a strict node contributes obligations even inside repetition.
The [canonical termination rule](match-traces.md) for common cycles is unchanged.

## Standalone boundary and migration

`A->$` still means end of traversal after A. It is not a strict-node suffix.
For example, `A->$B$->C->$` requires both sides of B to be covered and C to have
no successor. The lexer distinguishes attached prefix `$B` from standalone `$`.

This is an intentional incompatible language change after tag `V0`:

| V0 intention | New spelling |
| --- | --- |
| Both sides strict: `B$` | `$B$` |
| Both sides strict, anchor 3: `B$3` | `$B$3` |
| Repeated both-sides strict: `(B$->C)+` | `($B$->C)+` |
| End assertion: `A->$` | Unchanged |

There is no automatic legacy interpretation or command-line compatibility flag.
Review stored pattern files; no graph JSON, node-definition or match-report schema
change is required. The old runtime bypassed some strict requirements on
degree-one nodes; the new coverage rule deliberately removes that exception.
V0 remains available under its original Git tag.

## Tests and generation

Tests cover the lexer, parser reuse/errors, root plans, generated IR, explicit
positive and negative graphs, numeric identities, nullable groups, cycles,
captures and serial/parallel CLI reports. Randomized graph tests use independently
specified adjacency and expected coverage rather than PGInterpreterIR as an oracle.

`RandomExpressionGenerator` now uses one seeded engine, including key selection,
quantifiers and directional markers. It generates at most three top-level paths,
three units per sequence and a configurable group depth:

```cpp
CorGIReg::RandomExpressionGenerator::setDictionary({"A", "B", "C"});
CorGIReg::RandomExpressionGenerator::setSeed(20261026);
auto pattern = CorGIReg::RandomExpressionGenerator::allExpr(2);
CorGIReg::RandomExpressionGenerator::clearDictionary();
```

The default depth is 3, the allowed range is 0..6, and depth zero disables groups.
Reseeding reproduces the sequence for the same generator configuration and C++
standard-library implementation. Invalid dictionary keys are rejected. This
generator is for sequential test/example construction; it is not a thread-safe
runtime service. Its seed does not seed PGInterpreterIR's separate repetition
choices. Syntactic validity does not guarantee that every generated pattern has
a matching graph under its strict and identity constraints.
