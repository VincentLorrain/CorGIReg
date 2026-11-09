#pragma once

#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <any>         // For std::any
#include <stdexcept>   // For exception handling
#include <algorithm>
#include <map>
#include "GraphAccess/Ports.hpp"


#include <sstream>//mmd export to mermaid


namespace CorGIReg {

class NodeNN : public std::enable_shared_from_this<NodeNN> {
public:
    using Ptr = std::shared_ptr<NodeNN>;
    using Dictionary = std::unordered_map<std::string, std::any>;

    NodeNN() = default;


    // Method to copy info_ data from another node
    void copyInfoFrom(const Ptr& other);

    // Accessors for parents and children
    const std::set<Ptr>& getParents() const;
    const std::set<Ptr>& getChildren() const;

    // These low-level methods update only this endpoint. Use GraphNN::addEdge
    // to add reciprocal links and include both endpoints in the graph.
    void addParent(const Ptr& parent);
    void addChild(const Ptr& child);
    void addConnection(const Ptr& child, EdgePorts ports);
    std::set<EdgePorts> getConnectionPorts(const Ptr& child) const;

    // Methods to remove parents and children
    void removeParent(const Ptr& parent);
    void removeChild(const Ptr& child);

    // Template method to set information stored in the node
    template<typename T>
    void setInfo(const std::string& key, const T& value);

    // Template method to get information stored in the node
    template<typename T>
    T getInfo(const std::string& key) const;

    // Overloaded getInfo with default value
    template<typename T>
    T getInfo(const std::string& key, const T& defaultValue) const;

    /**
     * @brief Find endpoints at an exact signed distance from this node.
     * @param delta Positive steps follow children; negative steps follow parents.
     * @param nodeSee Nodes already visited on the current path.
     * @return Unique endpoints, with no repeated node on a path.
     */
    std::set<Ptr> getNodeDelta(int delta, std::set<Ptr> nodeSee);

    /**
     * @brief Merges the info_ dictionary from another node into this node.
     * @param otherNode The node from which to merge info_.
     */
    void mergeInfoFrom(const Ptr& otherNode);



    bool hasArgument(const std::string& arg) const {
        return info_.find(arg) != info_.end();
    }

    std::string getType() const;
    const Dictionary& getInfoDictionary() const { return info_; }

private:
    std::set<Ptr> parents_;
    std::set<Ptr> children_;
    Dictionary info_;
    std::map<Ptr, std::set<EdgePorts>> connectionPorts_;
};

/**
 * @brief GraphNN class representing a graph of NodeNN nodes
 */
class GraphNN {
public:
    // Method to add a node to the graph
    void addNode(const NodeNN::Ptr node);

    // Method to add an edge between two nodes
    void addEdge(const NodeNN::Ptr from, const NodeNN::Ptr to, EdgePorts ports = {});

    // Accessor for all nodes in the graph
    const std::vector<NodeNN::Ptr>& getNodes() const;

    /**
     * @brief Copy the graph's nodes and topology, reusing any excluded nodes.
     * @param nodesNotToCopy A set of nodes that should not be deep copied.
     * @return The copied graph. Excluded nodes are shared with the original.
     */
    std::shared_ptr<GraphNN> deepCopy(const std::unordered_set<NodeNN::Ptr>& nodesNotToCopy) const;


    /**
     * @brief Merges a set of nodes in the graph into a single node, combining their info_ dictionaries.
     * @param nodesToMerge A set of NodeNN::Ptr representing the nodes to be merged.
     */
    void mergeNodes(const std::set<NodeNN::Ptr>& nodesToMerge);


    /**
     * @brief string representation of the graph
     * @return string
     */
    std::string exportToMermaid() const ;


private:
    std::vector<NodeNN::Ptr> nodes_;
    std::unordered_set<NodeNN::Ptr> nodeSet_; // For quick lookup to prevent duplicates

    // Helper method to copy nodes
    NodeNN::Ptr _copyNode(const NodeNN::Ptr& node,
                         std::unordered_map<NodeNN::Ptr, NodeNN::Ptr>& copiedNodes,
                         const std::unordered_set<NodeNN::Ptr>& nodesNotToCopy) const;
};

// Template method definitions

template<typename T>
void NodeNN::setInfo(const std::string& key, const T& value) {
    info_[key] = value;
}

template<typename T>
T NodeNN::getInfo(const std::string& key) const {
    auto it = info_.find(key);
    if (it != info_.end()) {
        try {
            return std::any_cast<T>(it->second);
        } catch (const std::bad_any_cast&) {
            throw std::runtime_error("Type mismatch for key '" + key + "'");
        }
    } else {
        throw std::runtime_error("Key not found: '" + key + "'");
    }
}

template<typename T>
T NodeNN::getInfo(const std::string& key, const T& defaultValue) const {
    auto it = info_.find(key);
    if (it != info_.end()) {
        try {
            return std::any_cast<T>(it->second);
        } catch (const std::bad_any_cast&) {
            return defaultValue;
        }
    } else {
        return defaultValue;
    }
}

} // namespace CorGIReg
