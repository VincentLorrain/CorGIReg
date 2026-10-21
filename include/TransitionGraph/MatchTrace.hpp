#pragma once

#include "GraphAccess/GraphView.hpp"
#include <vector>

namespace CorGIReg {

struct NodeCapture {
    NodeId node;
    std::string predicate;
};

// A graph may have several contributing walks; no total ordering is imposed
// between independent branches. Within a walk, captures follow execution order.
struct MatchWalk {
    std::set<std::size_t> origins;
    std::vector<NodeCapture> nodes;
};

} // namespace CorGIReg
