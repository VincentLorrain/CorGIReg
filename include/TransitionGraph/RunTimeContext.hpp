#pragma once
#include <memory>
#include <optional>
#include "GraphAccess/GraphView.hpp"
#include "TransitionGraph/MatchTrace.hpp"
namespace CorGIReg {
class State;
struct MatchEnvironment {
    std::shared_ptr<const GraphView> graph;
    NodePredicates predicates;
};
class RunTimeContext {
    // Obligations belong to this walk; an abandoned alternative must not leak
    // requirements into skipped quantifiers or other successful alternatives.
    NodeIds mRejectedNodes;
    std::shared_ptr<const MatchEnvironment> mEnvironment;
    std::shared_ptr<State> mActState;
    std::optional<NodeId> mActOpNode;
    std::map<NodeId, std::size_t> mCommonNodes;
    std::map<std::string, NodeIds> mValidNodes;
    std::vector<NodeCapture> mCaptures;
    std::vector<QuantifierCapture> mQuantifiers;
    std::vector<std::pair<std::size_t, std::size_t>> mActiveQuantifiers;
public:
    RunTimeContext(std::shared_ptr<State> state, std::optional<NodeId> node,
                   std::shared_ptr<const MatchEnvironment> environment);
    RunTimeContext(std::shared_ptr<RunTimeContext> context);
    RunTimeContext(std::shared_ptr<RunTimeContext> context,
                   std::shared_ptr<State> state, std::optional<NodeId> node);
    const GraphView& graph() const { return *mEnvironment->graph; }
    bool testNode(const std::string& key, NodeId node) const {
        auto found = mEnvironment->predicates.find(key);
        return found != mEnvironment->predicates.end() && found->second(node);
    }
    void addRejectedNode(NodeId node) { mRejectedNodes.insert(node); }
    NodeIds getRejectedNodes() const { return mRejectedNodes; }
    std::optional<NodeId> getActNode() const { return mActOpNode; }
    const std::map<std::string, NodeIds>& getValid() const { return mValidNodes; }
    const auto& getCaptures() const noexcept { return mCaptures; }
    const auto& getQuantifiers() const noexcept { return mQuantifiers; }
    void beginQuantifier(std::size_t index, char symbol);
    bool completeIteration(std::size_t index);
    void endQuantifier(std::size_t index);
    void setCommon(NodeId node, std::size_t commonIdx);
    std::optional<NodeId> getCommonNodeFromIdx(const std::size_t& commonIdx);
    NodeIds getCommonNodes() const;
    bool isCommonDefined(NodeId node);
    std::set<std::size_t> getSubStmId() const;
    bool areEqual(const std::shared_ptr<RunTimeContext>& context) const;
    bool areCompatible(const std::shared_ptr<RunTimeContext>& context) const;
    bool isOnValidState();
    NodeIds getValidNodes() const;
    bool isAlreadyValid(NodeId node);
    void setValid(NodeId node, const std::string tag);
    NodeIds getValidNodesNoCommon() const;
    std::shared_ptr<State> getActState();
};
} // namespace CorGIReg
