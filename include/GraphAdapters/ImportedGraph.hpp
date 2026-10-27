#pragma once
#include "GraphAccess/GraphView.hpp"
#include <stdexcept>

namespace CorGIReg {

// Format-neutral operation graph, populated by file importers.
class ImportedGraph final : public GraphView {
public:
    struct Node { std::string predicate, name; NodeIds parents, children; };
    void addNode(NodeId id, std::string predicate, std::string name) {
        if (!nodes.emplace(id, Node{std::move(predicate), std::move(name), {}, {}}).second)
            throw std::invalid_argument("Duplicate source node ID");
    }
    void addEdge(NodeId from, NodeId to, EdgePorts ports = {}) {
        ports.validate();
        auto& source = nodes.at(from);
        auto& target = nodes.at(to);
        source.children.insert(to);
        target.parents.insert(from);
        connections[{from, to}].insert(ports);
    }
    std::set<EdgePorts> edgePorts(NodeId from, NodeId to) const override {
        nodes.at(from); nodes.at(to);
        auto found = connections.find({from, to});
        return found == connections.end() ? std::set<EdgePorts>{} : found->second;
    }
    NodeIds parents(NodeId id) const override { return nodes.at(id).parents; }
    NodeIds children(NodeId id) const override { return nodes.at(id).children; }
    std::string nodeName(NodeId id) const override { return nodes.at(id).name; }
    const auto& getNodes() const noexcept { return nodes; }
private:
    std::map<NodeId, Node> nodes;
    std::map<std::pair<NodeId, NodeId>, std::set<EdgePorts>> connections;
};

} // namespace CorGIReg
