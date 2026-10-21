#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>

namespace CorGIReg {

// IDs belong to one view and remain stable while that view is being searched.
// Zero is a valid ID; absence of a node is represented separately.
using NodeId = std::uint64_t;
using NodeIds = std::set<NodeId>;
using NodePredicates = std::map<std::string, std::function<bool(NodeId)>>;

// Read-only topology. Implementations must return unique, reciprocal neighbors
// and reject unknown IDs. The graph must not change during a search.
class GraphView {
public:
    virtual ~GraphView() = default;
    virtual NodeIds parents(NodeId node) const = 0;
    virtual NodeIds children(NodeId node) const = 0;
    // Optional presentation metadata. IDs, not names, establish identity.
    virtual std::string nodeName(NodeId node) const { return std::to_string(node); }
};

} // namespace CorGIReg
