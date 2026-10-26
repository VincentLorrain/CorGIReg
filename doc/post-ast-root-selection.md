# Post-AST root selection proposal

The compilation pipeline now has an explicit planning phase:

```text
PartitionGraphLexer -> PartitionGraphParser -> original AST
    -> RootSelectionPass -> proposed root plan
    -> PGInterpreterTG::visit(original AST) -> TransitionGraph -> TGCompiled
```

This commit selects roots and exposes a plan; it does **not** yet generate a
rerooted machine. Existing starts, captures, quantifier indices and CLI search
budgets retain their behavior. In particular, it does not yet remove the
Cartesian enumeration in the CLI. The separation allows review of root selection
before changing execution semantics.

## Representation and choice

`CompileTool/PostAstOptimization/RootSelectionPass` is an independent pass with
no graph-runtime or NodeDictionary dependency. It consumes the AST and produces:

- Planning nodes retaining references to their original AST occurrences and
  the lexical paths (`;`-separated sequences) in which they appear.
- Directed constraints between these nodes. `PREC` respects the interpreter's
  current sequence semantics: the entire right operand precedes the left one.
- Undirected connected components, each with an optional proposed root and an
  explicit selection status.

Unquantified common occurrences share an identity based on predicate and numeric
suffix. `B#1`, `$B#1`, `B$1` and `$B$1` refer to the same anchor, but each
occurrence retains its input/output strictness. `B#` and `B#0` remain distinct. Repeated ordinary `B` occurrences
do not acquire shared identity merely because their predicate names match.

The pass ranks eligible nodes by:

1. Number of distinct lexical paths sharing the node.
2. Number of distinct neighboring planning nodes.
3. First lexical occurrence, as a stable tie-breaker.

This is a static heuristic, not a claim about selectivity in a particular input
graph. Plan-local indices make it reproducible across compilations and test order.
Edge direction is preserved; connected-component discovery alone treats edges
as undirected. Cycles and self-constraints remain in the edge list.

Examples:

| Pattern | Proposal |
| --- | --- |
| `A+->B#;B#->C` | One component, root `B#` |
| `A->B#;B#->C#;C#->D` | One component, root `B#` after deterministic tie-break |
| `B#<-A;C<-B#` | One component, root `B#`, constraints A to B and B to C |
| `A->B;C->D` | Two components, roots A and C |
| `A;A` | Two components, two distinct ordinary nodes |
| `A*->B` | Root B; optional A remains an opaque planning unit |
| `A+` | No eligible root in this first version |

## Quantifiers and conservative limits

A quantified expression is kept as a single opaque unit, with the complete
original subtree. It is never selected as a root, including `+`: selecting a
particular occurrence inside a repeated region needs a later analysis. `$` is
an end assertion rather than a source node, so it is also ineligible.

If a common node appears inside any quantified expression, the whole plan is
marked `hasQuantifiedCommon` and component statuses become
`QuantifiedCommonNeedsAnalysis`, with no selected roots. The listed components
then describe only the outer skeleton and must **not** be treated as independent
matches: hidden anchors may connect them with iteration-dependent semantics.
This conservative fallback preserves all currently supported patterns.

## Integration and next phase

```cpp
CorGIReg::PGInterpreterTG interpreter("A+->B#;B#->C");
auto compiled = interpreter.interpret();
const auto& proposal = interpreter.getRootSelectionPlan();
// proposal.components[0].root indexes proposal.nodes.
// compiled still uses the original generation strategy.
```

Before `interpret()`, the plan is empty. Each call replaces it with a fresh plan.
The plan owns its AST references; callers may copy it to retain it independently
of the interpreter. No AST nodes or query text are rewritten.

The next phase should consume a selected component root to generate parent and
child exploration, preserve non-tree edges as consistency checks, and map all
captures back to their original occurrences. It must handle nullable sequences,
shared/strict constraints and quantifier traces before enabling that execution
strategy. Until then the proposal is inspectable without changing match results.
