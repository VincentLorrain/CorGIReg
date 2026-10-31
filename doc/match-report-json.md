# Match report JSON (schema version 1)

The public `corgireg` CLI writes one UTF-8 JSON object to stdout or `-o FILE`.
An empty `matches` array is a successful search with no match, not an import
error. This report is a result IR, not a new pattern or a transformed network.

| Top-level field | Meaning |
| --- | --- |
| `schema_version` | Integer `1`, version of this public report. |
| `query` | Complete original pattern, including quantifiers and whitespace. |
| `source` | Absolute input-file path. |
| `format` | `json`, `onnx` or `stablehlo`. |
| `graph` | Entire normalized source [operation graph](graph-json.md), including unmatched nodes. |
| `matches` | Array of successful solutions, described below. |
| `node_definitions` | Present when a node library is supplied; definition manifest keyed by predicate name. |

## Solutions, walks and quantifiers

Each `matches[]` object has:

| Field | Meaning |
| --- | --- |
| `node_ids` | Unique captured source IDs, sorted numerically. |
| `starts` | Source ID or null for each compiled start state. Null represents absence, never ID zero. |
| `walks` | Successful walks; no total ordering between separate branches is guaranteed. |

`starts` follows compiled state order, which is not guaranteed to be the textual
order of semicolon-separated paths. That order is the creation order of the start
states, so the same pattern and graph produce the same `starts` and match order
on every run. When a component is searched from its anchor
root, each of its paths becomes walks that start on the anchor: `starts` repeats
that node and walks list captures from the anchor outward (see
[root selection](post-ast-root-selection.md)). Each walk contains:

| Field | Meaning |
| --- | --- |
| `origins` | Internal automaton origin IDs. Not source-node IDs or stable identifiers across compilations. |
| `nodes` | Captures in execution order; repeated visits may repeat a source ID. |
| `nodes[].id` | Original source-node ID. |
| `nodes[].name` | Source name or decimal fallback. |
| `nodes[].predicate` | Predicate used by the pattern, including semantic aliases and inline conditions, e.g. `conv(group=1)`. |
| `quantifiers` | Quantifier invocations in execution order, including skipped invocations. |

Each `quantifiers[]` object contains:

| Field | Meaning |
| --- | --- |
| `index` | One-based lexical position of the quantifier symbol in `query`. |
| `symbol` | `+`, `*` or `?`. |
| `count` | Number of completed iterations of the whole body. |
| `parent` | Null, or zero-based position of the enclosing invocation in this walk's `quantifiers`. |
| `begin`, `end` | Half-open range `[begin, end)` within this walk's `nodes`. |
| `iteration_ends` | Exclusive node-capture offset at each iteration end; its length equals `count`. |

For the [example graph](examples/graph.json), a solution for `A+->B` contains:

```json
{
  "node_ids": [0, 9, 42],
  "starts": [42],
  "walks": [{
    "origins": [0],
    "nodes": [
      {"id": 42, "name": "first_A", "predicate": "A"},
      {"id": 0, "name": "second_A", "predicate": "A"},
      {"id": 9, "name": "last_B", "predicate": "B"}
    ],
    "quantifiers": [{
      "index": 1, "symbol": "+", "count": 2, "parent": null,
      "begin": 0, "end": 2, "iteration_ends": [1, 2]
    }]
  }]
}
```

The origin value above is illustrative and must not be hard-coded by consumers.
The search also returns a suffix solution starting at ID 0 with count 1.
For nested repetitions, one textual quantifier can have several invocations.
Use `index` for textual identity and `parent` for invocation nesting; see the
[nested example and C++ mapping](match-traces.md).

## Node-definition manifest

Each `node_definitions[name]` object records `description` (string), `source`
(absolute definition-file path), `openmath` (the definition's `{cd, name, cdbase?}`
OpenMath symbol, or null), `format` (selected input format), `rule`
(selected `{op_types, where?}` object), and `node_ids` (filtered source IDs).
These IDs describe all nodes satisfying the definition, not only captured ones.
Schema 4 definitions also include `key_matching`. Inline occurrence conditions
filter matches further; the manifest's `node_ids` still describe the base
definition's `op_types` and `where` membership.
For an unused definition lacking a mapping for this format, `rule` and
`node_ids` are null. An available definition with no accepted nodes has `[]`.
A referenced definition with no format mapping is an error.

Definition schema versions 1/2/3/4 and internal wire versions 1 through 6 are independent
of report schema version 1. See [node libraries](node-library.md) and the
[native protocol](native-protocol.md).

## Interpretation limits and failures

Solutions are not ranked or guaranteed greedy. Different quantifier allocations
may produce the same `node_ids`; deduplicate only if your application deliberately
discards those distinctions. The same iterations in another order are reported
once (see [traces](match-traces.md)). Different start tuples can still report
the same nodes, for example when a repeated anchor prevents rooted search. There is no explicit matched-edge list: filtering
`graph.edges` to captured endpoints produces an induced subgraph, which can
contain edges that were not traversed. Names are display labels; IDs identify
operations within the source graph.

For `A+->B#; B#->C` against the example, no C exists and `matches` is empty.
The report's `graph` remains the input graph; the matcher never invents C.
Import, rule, syntax, search-budget and timeout errors produce diagnostics on
stderr and CLI exit code 2. Successful searches, including no match, return 0.
Output files are replaced atomically only after success; their parent directory
must already exist. Input graphs, pattern files and loaded definitions cannot
be used as output destinations.
