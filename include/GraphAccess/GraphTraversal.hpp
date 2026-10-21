#pragma once

#include "GraphAccess/GraphView.hpp"
#include <vector>
#include <utility>

namespace CorGIReg {

// Exact signed path length, not shortest-path distance. A node cannot occur
// twice on the same path; different paths keep independent visited sets.
inline NodeIds nodesAtDelta(const GraphView& graph, NodeId anchor, int delta,
                           NodeIds visited = {}) {
    struct Walk { NodeId node; int remaining; NodeIds visited; };
    std::vector<Walk> pending{{anchor, delta, std::move(visited)}};
    NodeIds result;
    while (!pending.empty()) {
        auto walk = std::move(pending.back());
        pending.pop_back();
        walk.visited.insert(walk.node);
        if (walk.remaining == 0) {
            result.insert(walk.node);
            continue;
        }
        const auto neighbors = walk.remaining > 0
            ? graph.children(walk.node) : graph.parents(walk.node);
        const int remaining = walk.remaining > 0
            ? walk.remaining - 1 : walk.remaining + 1;
        for (NodeId neighbor : neighbors) {
            if (!walk.visited.count(neighbor)) {
                pending.push_back({neighbor, remaining, walk.visited});
            }
        }
    }
    return result;
}

} // namespace CorGIReg
