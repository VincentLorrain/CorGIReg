#pragma once
#include "NodeTest/NodeRule.hpp"

namespace CorGIReg {
// One node occurrence, including optional equality conditions and anchor markers.
struct NodePattern {
    std::string name, predicate, anchor;
    std::vector<std::pair<std::string, NodeValue>> conditions;
};

// Values are JSON literals (strings must be quoted; arrays use brackets).
// Throws invalid_argument for malformed or duplicate conditions.
NodePattern parseNodePattern(const std::string& text);
}
