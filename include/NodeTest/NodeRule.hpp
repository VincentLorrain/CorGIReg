#pragma once

#include "BasicGraphIr/Graph.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <variant>

namespace CorGIReg {

// A format-independent document. Null is a value; absence is a failed path lookup.
struct NodeValue {
    struct Missing {};
    struct Unsupported { bool operator==(const Unsupported&) const { return true; } };
    using Array = std::vector<NodeValue>;
    using Object = std::map<std::string, NodeValue>;
    using Data = std::variant<std::nullptr_t, bool, std::int64_t, std::uint64_t,
                              double, std::string, Array, Object, Unsupported, Missing>;
    Data data = nullptr;
    NodeValue() = default;
    NodeValue(std::nullptr_t) : data(nullptr) {}
    NodeValue(bool value) : data(value) {}
    NodeValue(int value) : data(std::int64_t(value)) {}
    NodeValue(std::int64_t value) : data(value) {}
    NodeValue(std::uint64_t value) : data(value) {}
    NodeValue(double value) : data(value) {}
    NodeValue(const char* value) : data(std::string(value)) {}
    NodeValue(std::string value) : data(std::move(value)) {}
    NodeValue(Array value) : data(std::move(value)) {}
    NodeValue(Object value) : data(std::move(value)) {}
    NodeValue(Unsupported value) : data(value) {}
    NodeValue(Missing value) : data(value) {}
};

using NodeRule = std::function<bool(const NodeValue&)>;

// Validate the entire rule and compile regexes once, before evaluating any nodes.
// Throws invalid_argument with a rule path for invalid syntax or operand types.
NodeRule compileNodeRule(const NodeValue& rule);

// NodeNN's info dictionary becomes attributes; Type also becomes op.
// Unsupported std::any types retain presence but cannot satisfy value comparisons.
NodeValue nodeDocument(const NodeNN& node);
}
