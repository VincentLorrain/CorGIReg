#pragma once
#include "GraphAdapters/BasicGraphAdapter.hpp"
#include <ostream>

namespace CorGIReg {
// Portable operation graph output. IDs follow GraphNN insertion order.
inline void writeGraphJson(const GraphNN& graph, std::ostream& output) {
    BasicGraphAdapter view(graph);
    auto string = [&](const std::string& value) {
        constexpr char hex[] = "0123456789abcdef";
        output << '"';
        for (unsigned char c : value) {
            if (c == '"' || c == '\\') output << '\\' << c;
            else if (c < 32) output << "\\u00" << hex[c >> 4] << hex[c & 15];
            else output << c;
        }
        output << '"';
    };
    output << "{\"nodes\":[";
    bool first = true;
    for (const auto& node : graph.getNodes()) {
        if (node->getType().empty()) throw std::invalid_argument("Graph export requires a nonempty node Type");
        if (!first) output << ',';
        first = false;
        const auto id = view.id(node);
        output << "{\"id\":" << id << ",\"op\":"; string(node->getType());
        output << ",\"name\":";
        string(node->getInfo<std::string>("Name", node->getType() + "_" + std::to_string(id)));
        output << '}';
    }
    output << "],\"edges\":["; first = true;
    for (const auto& node : graph.getNodes()) for (auto child : view.children(view.id(node))) {
        if (!first) output << ',';
        first = false;
        output << '[' << view.id(node) << ',' << child << ']';
    }
    output << "],\"connections\":["; first = true;
    for (const auto& node : graph.getNodes()) for (auto child : view.children(view.id(node)))
        for (const auto& ports : view.edgePorts(view.id(node), child)) {
            if (!first) output << ',';
            first = false;
            output << "{\"from\":" << view.id(node) << ",\"to\":" << child << ",\"input\":";
            if (ports.input) output << *ports.input; else output << "null";
            output << ",\"output\":";
            if (ports.output) output << *ports.output; else output << "null";
            output << '}';
        }
    output << "]}\n";
    if (!output) throw std::runtime_error("Cannot write graph JSON");
}
}
