#pragma once

#include "BasicGraphIr/Graph.hpp"
#include "GraphAccess/GraphView.hpp"

namespace CorGIReg {

// Keeps source nodes alive without copying their attributes or connections.
// GraphNN construction preserves getNodes() indices; the roots constructor
// discovers the connected component for the legacy matching entry point.
class BasicGraphAdapter final : public GraphView {
public:
    explicit BasicGraphAdapter(const GraphNN& graph) : BasicGraphAdapter(graph.getNodes()) {}
    explicit BasicGraphAdapter(const std::vector<NodeNN::Ptr>& roots) {
        for (const auto& root : roots) if (root) add(root);
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            const auto node = nodes_[i];
            for (const auto& parent : node->getParents()) add(parent);
            for (const auto& child : node->getChildren()) add(child);
        }
    }

    NodeIds parents(NodeId id) const override { return ids(node(id)->getParents()); }
    NodeIds children(NodeId id) const override { return ids(node(id)->getChildren()); }
    std::set<EdgePorts> edgePorts(NodeId from, NodeId to) const override {
        return node(from)->getConnectionPorts(node(to));
    }
    const NodeNN::Ptr& node(NodeId id) const { return nodes_.at(id); }
    NodeId id(const NodeNN::Ptr& node) const { return ids_.at(node); }

private:
    void add(const NodeNN::Ptr& node) {
        if (!node) throw std::invalid_argument("Null graph neighbor");
        if (ids_.emplace(node, nodes_.size()).second) nodes_.push_back(node);
    }
    NodeIds ids(const std::set<NodeNN::Ptr>& nodes) const {
        NodeIds result;
        for (const auto& node : nodes) result.insert(id(node));
        return result;
    }
    std::vector<NodeNN::Ptr> nodes_;
    std::map<NodeNN::Ptr, NodeId> ids_;
};

} // namespace CorGIReg
