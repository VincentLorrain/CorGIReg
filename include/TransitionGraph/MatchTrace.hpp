#pragma once

#include "GraphAccess/GraphView.hpp"
#include <vector>
#include <optional>
#include <tuple>

namespace CorGIReg {

struct NodeCapture {
    NodeId node;
    std::string predicate;
    bool operator==(const NodeCapture& other) const {
        return node == other.node && predicate == other.predicate;
    }
};

struct QuantifierCapture {
    std::size_t index; // One-based, left-to-right order of the symbols in the query.
    char symbol;
    std::optional<std::size_t> parent; // Index in this walk's invocation vector.
    std::size_t begin; // Half-open range in MatchWalk::nodes.
    std::size_t end;
    std::vector<std::size_t> iterationEnds;
    std::size_t count() const noexcept { return iterationEnds.size(); }
    bool operator==(const QuantifierCapture& other) const {
        return std::tie(index, symbol, parent, begin, end, iterationEnds) ==
               std::tie(other.index, other.symbol, other.parent, other.begin, other.end, other.iterationEnds);
    }
};

// A graph may have several contributing walks; no total ordering is imposed
// between independent branches. Within a walk, captures follow execution order.
struct MatchWalk {
    std::set<std::size_t> origins;
    std::vector<NodeCapture> nodes;
    // Invocation order, including zero occurrences; nested invocations have a parent.
    std::vector<QuantifierCapture> quantifiers;
};

} // namespace CorGIReg
