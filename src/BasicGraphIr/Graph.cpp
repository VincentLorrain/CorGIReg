#include "BasicGraphIr/Graph.hpp"

namespace CorGIReg {

// NodeNN Class Implementations


void NodeNN::copyInfoFrom(const Ptr& other) {
    info_ = other->info_;
}

const std::set<NodeNN::Ptr>& NodeNN::getParents() const {
    return parents_;
}

const std::set<NodeNN::Ptr>& NodeNN::getChildren() const {
    return children_;
}

void NodeNN::addParent(const Ptr& parent) {
    parents_.insert(parent);
}

void NodeNN::addChild(const Ptr& child) {
    children_.insert(child);
}

void NodeNN::removeParent(const Ptr& parent) {
    parents_.erase(parent);
}

void NodeNN::removeChild(const Ptr& child) {
    children_.erase(child);
}
std::string NodeNN::getType() const {
    return getInfo<std::string>("type", "");
}

std::set<NodeNN::Ptr> NodeNN::getNodeDelta(int delta, std::set<Ptr> nodeSee) {
    std::set<Ptr> out;
    nodeSee.insert(shared_from_this());

    if (delta == 0) {
        out.insert(shared_from_this());
    } else if (delta > 0) {
        for (const Ptr& node : getChildren()) {
            if (nodeSee.find(node) == nodeSee.end()) {  // Loop avoidance
                auto childNodes = node->getNodeDelta(delta - 1, nodeSee);
                out.insert(childNodes.begin(), childNodes.end());
            }
        }
    } else {
        for (const Ptr& node : getParents()) {
            if (nodeSee.find(node) == nodeSee.end()) {  // Loop avoidance
                auto parentNodes = node->getNodeDelta(delta + 1, nodeSee);
                out.insert(parentNodes.begin(), parentNodes.end());
            }
        }
    }

    return out;
}


void NodeNN::mergeInfoFrom(const Ptr& otherNode) {
    for (const auto& pair : otherNode->info_) {
        const std::string& key = pair.first;
        const std::any& value = pair.second;

        // If key does not exist, add it. If it exists, overwrite it with the new value.
        info_[key] = value;
    }
}

// GraphNN Class Implementations

void GraphNN::addNode(const NodeNN::Ptr node) {
    if (nodeSet_.find(node) == nodeSet_.end()) {
        nodes_.push_back(node);
        nodeSet_.insert(node);
    }
}

void GraphNN::addEdge(const NodeNN::Ptr from, const NodeNN::Ptr to) {
    // Add nodes to graph if not already present
    addNode(from);
    addNode(to);

    // Add edge between nodes
    from->addChild(to);
    to->addParent(from);
}

const std::vector<NodeNN::Ptr>& GraphNN::getNodes() const {
    return nodes_;
}


// Deep copy method
std::shared_ptr<GraphNN> GraphNN::deepCopy(const std::unordered_set<NodeNN::Ptr>& nodesNotToCopy) const {
    // Create a new GraphNN instance
    auto newGraph = std::make_shared<GraphNN>();

    // Map from original nodes to copied nodes
    std::unordered_map<NodeNN::Ptr, NodeNN::Ptr> copiedNodes;

    // Initialize the map with nodes not to copy, mapping them to themselves
    for (const auto& node : nodesNotToCopy) {
        copiedNodes[node] = node;
    }

    // Copy each node in the graph
    for (const auto& node : nodes_) {
        auto copiedNode = _copyNode(node, copiedNodes, nodesNotToCopy);
        newGraph->addNode(copiedNode);
    }

    return newGraph;
}

// Helper method to copy a node
NodeNN::Ptr GraphNN::_copyNode(const NodeNN::Ptr& node,
                              std::unordered_map<NodeNN::Ptr, NodeNN::Ptr>& copiedNodes,
                              const std::unordered_set<NodeNN::Ptr>& nodesNotToCopy) const {
    // Check if the node has already been copied
    auto it = copiedNodes.find(node);
    if (it != copiedNodes.end()) {
        return it->second;
    }

    // If the node should not be copied, map it to itself
    if (nodesNotToCopy.find(node) != nodesNotToCopy.end()) {
        copiedNodes[node] = node;
        return node;
    }

    // Create a new node
    auto newNode = std::make_shared<NodeNN>();

    // Copy the info_ data (assuming info_ is accessible)
    newNode->copyInfoFrom(node); 

    // Map the original node to the new node
    copiedNodes[node] = newNode;

    // Copy and set parents
    for (const auto& parent : node->getParents()) {
        auto copiedParent = _copyNode(parent, copiedNodes, nodesNotToCopy);
        newNode->addParent(copiedParent);
    }

    // Copy and set children
    for (const auto& child : node->getChildren()) {
        auto copiedChild = _copyNode(child, copiedNodes, nodesNotToCopy);
        newNode->addChild(copiedChild);
    }

    return newNode;
}



void GraphNN::mergeNodes(const std::set<NodeNN::Ptr>& nodesToMerge) {
    if (nodesToMerge.empty()) {
        return; // Nothing to merge
    }

    // Create a new node
    NodeNN::Ptr mergedNode = std::make_shared<NodeNN>();

    // Merge info_ dictionaries
    for (const auto& node : nodesToMerge) {
        mergedNode->mergeInfoFrom(node);
    }

    // Keep track of all unique parents and children
    std::set<NodeNN::Ptr> allParents;
    std::set<NodeNN::Ptr> allChildren;

    // Collect all parents and children of nodes being merged
    for (const auto& node : nodesToMerge) {
        for (const auto& parent : node->getParents()) {
            // If the parent is not in nodesToMerge
            if (nodesToMerge.find(parent) == nodesToMerge.end()) {
                allParents.insert(parent);
            }
        }
        for (const auto& child : node->getChildren()) {
            // If the child is not in nodesToMerge
            if (nodesToMerge.find(child) == nodesToMerge.end()) {
                allChildren.insert(child);
            }
        }
    }

    // Adjust parents
    for (const auto& parent : allParents) {
        // Remove edges from parent to nodes being merged
        for (const auto& node : nodesToMerge) {
            parent->removeChild(node);
            node->removeParent(parent);
        }
        // Add edge from parent to mergedNode
        parent->addChild(mergedNode);
        mergedNode->addParent(parent);
    }

    // Adjust children
    for (const auto& child : allChildren) {
        // Remove edges from nodes being merged to child
        for (const auto& node : nodesToMerge) {
            node->removeChild(child);
            child->removeParent(node);
        }
        // Add edge from mergedNode to child
        mergedNode->addChild(child);
        child->addParent(mergedNode);
    }

    // Remove internal edges among nodes being merged
    for (const auto& node : nodesToMerge) {
        // Remove all parents and children that are in nodesToMerge
        std::set<NodeNN::Ptr> internalParents;
        std::set<NodeNN::Ptr> internalChildren;

        for (const auto& parent : node->getParents()) {
            if (nodesToMerge.find(parent) != nodesToMerge.end()) {
                internalParents.insert(parent);
            }
        }
        for (const auto& child : node->getChildren()) {
            if (nodesToMerge.find(child) != nodesToMerge.end()) {
                internalChildren.insert(child);
            }
        }

        for (const auto& parent : internalParents) {
            node->removeParent(parent);
        }
        for (const auto& child : internalChildren) {
            node->removeChild(child);
        }
    }

    // Remove nodes being merged from the graph
    for (const auto& node : nodesToMerge) {
        // Remove node from nodes_ vector
        nodes_.erase(std::remove(nodes_.begin(), nodes_.end(), node), nodes_.end());
        // Remove node from nodeSet_
        nodeSet_.erase(node);
    }

    // Add mergedNode to the graph
    addNode(mergedNode);
}

} // namespace CorGIReg
