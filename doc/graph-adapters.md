# Matching an existing graph

CorGIReg can run its automaton over a read-only `GraphView`. The source graph
does not need to inherit from `NodeNN`, copy its attributes into `std::any`, or
implement graph construction and merging.

## Adapter contract

For serialized inputs and output fields, see [graph JSON](graph-json.md) and
[match reports](match-report-json.md). Explicit starting tuples must follow
`getStartStates()` order; it is not guaranteed to be lexical path order.

Implement the two methods from `GraphAccess/GraphView.hpp`:

```cpp
NodeIds parents(NodeId node) const override;
NodeIds children(NodeId node) const override;
```

- `NodeId` is an unsigned integer scoped to one view; zero is a valid node.
- IDs must remain stable and the graph must remain unchanged during a search.
- Return unique neighboring nodes, with reciprocal parent/child relationships.
- Reject unknown IDs, for example with `std::out_of_range`.
- An adapter must keep its source representation alive for as long as needed.
  The matcher retains the view with the returned results.
- Multiple uses of one producer are one neighbor in this topology interface.
  Use optional port metadata to distinguish the operand bindings.

For numbered patterns, also override:

```cpp
std::set<EdgePorts> edgePorts(NodeId from, NodeId to) const override;
```

`EdgePorts{input, output}` contains optional one-based 32-bit port numbers.
Return one binding per distinct operand use. The default implementation returns
an unspecified binding for adjacent nodes and no bindings for non-neighbors,
so existing adapters remain compatible with plain/wildcard arrows.
`ImportedGraph::addEdge(from, to, {1, 2})` connects source output 2 to destination
input 1. `GraphNN::addEdge` accepts the same optional third argument, and both
`BasicGraphAdapter` constructors expose those bindings. Ports survive graph
copies and merges. Node degree and strict coverage still count neighboring nodes,
not the number of port bindings. A fixed pattern input never matches an
unspecified source port.

`nodesAtDelta(view, anchor, delta)` implements exact signed traversal: positive
steps follow children, negative steps follow parents, and zero returns the
anchor. Paths cannot revisit a node. Visited nodes are tracked independently
for each path; this is not a shortest-path-distance query. Supply a valid anchor.

## A graph with integer source IDs

```cpp
#include "GraphAccess/GraphView.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"

using namespace CorGIReg;

class NetworkView final : public GraphView {
public:
    NodeIds parents(NodeId id) const override { return incoming.at(id); }
    NodeIds children(NodeId id) const override { return outgoing.at(id); }

    std::map<NodeId, NodeIds> incoming{{0, {}}, {42, {0}}};
    std::map<NodeId, NodeIds> outgoing{{0, {42}}, {42, {}}};
};

int main() {
    auto network = std::make_shared<NetworkView>();
    NodePredicates predicates{
        {"First", [](NodeId id) { return id == 0; }},
        {"Last",  [](NodeId id) { return id == 42; }}
    };
    auto pattern = PGInterpreterTG("First->Last").interpret();
    auto result = pattern->test(network, {0}, predicates);
    for (const auto& match : result->getMatches()) {
        auto firstIds = match->at("First"); // {0}
        auto allIds = match->getAll();      // {0, 42}, original source IDs
    }
    return result->isMatch() ? 0 : 1;
}
```

In real adapters, predicates may capture the source view and inspect the native
operation's type or attributes. No `NodeDictionary` registration is required
for this overload. Predicates are bound per search: a compiled pattern can be
reused with another view and another predicate table. A missing key rejects the
corresponding transition. Predicate and adapter exceptions propagate to callers.

The start vector is ordered and must have `getNbStart()` entries, corresponding
to `getStartStates()`. `std::nullopt` represents absence at a graph boundary;
it is different from node ID zero. This API searches from supplied starting
points; it does not yet implement automatic global search.

## Existing BasicGraph users

The existing `pattern->test({nodeA})` entry point still accepts `NodeNN` pointers
and uses registered `NodeDictionary` predicates. It captures available legacy
predicates when the automaton is compiled. The wrapper builds a
`BasicGraphAdapter` over the connected nodes and delegates to the same engine.

For repeated searches, construct an explicit `BasicGraphAdapter` and supply
ID-based predicates to avoid rebuilding the index for every call. When built
from `GraphNN`, its initial IDs follow `getNodes()` order. The roots constructor
assigns IDs while discovering connected nodes; these are view-local indices,
not persistent model IDs. Neither constructor copies attributes or connections.

Captures now contain `NodeId` values instead of `shared_ptr<NodeNN>` values.
Code using `MatchSolution::at`, `getAll` or `getStartNode` must account for this
API change. Resolve legacy results back to their original objects with:

```cpp
auto source = std::dynamic_pointer_cast<const BasicGraphAdapter>(result->getGraph());
// Include "GraphAdapters/BasicGraphAdapter.hpp" for this adapter.
if (source) {
    for (const auto& match : result->getMatches()) {
        for (NodeId id : match->getAll()) {
            NodeNN::Ptr original = source->node(id);
        }
    }
}
```

IDs from different views must not be compared as if they referred to one graph.
Use the same view across searches when comparing captures from those searches.
The existing UNIQUE degree checks and COMMON/STRICTCOMMON behavior are retained.
This change does not make the compiler or legacy global registries thread-safe.

## Adding an IR adapter

See [match traces](match-traces.md) for the complete query, ordered source captures,
source names and the actual counts of `+`, `*` and `?` in each result.

Put format-specific code in `GraphAdapters/`. For ONNX, an adapter can index
operations and map value producers to consumers. For StableHLO, an adapter can
index native operations and expose operand/use relationships within an explicitly
chosen region. Keep format-specific loading, ownership and source lookup there.
The [CLI](cli.md) includes file readers in `python/corgireg_cli/import_onnx.py`
and `import_stablehlo.py`. They pass an operation graph to `ImportedGraph` through
the native worker. The CLI documents the supported regions and source ID mapping;
in-process adapters can still expose native objects directly through `GraphView`.

Tests in `test/GraphAccess` cover topology and signed traversal. Tests in
`test/App/external_graph_test.cpp` use an independent integer graph and verify
captures, direction, common anchors, strict coverage, repetition, absence and
lifetime. `test/App/basic_graph_adapter_test.cpp` checks parity with the legacy
entry point using explicit source graphs.
