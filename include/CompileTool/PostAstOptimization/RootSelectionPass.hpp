#pragma once

#include "CompileTool/Common/AstNode.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphTokens.hpp"
#include <optional>
#include <set>

namespace CorGIReg {

using PatternAst = AstNode<tGTokensTypes>;

// A planning unit retains its original syntax, including quantifiers and anchors.
struct RootPlanNode {
    std::vector<std::shared_ptr<const PatternAst>> occurrences;
    std::set<std::size_t> paths;
    bool rootEligible = false;
};

struct RootPlanEdge {
    std::size_t source;
    std::size_t target;
};

enum class RootSelectionStatus {
    Selected,
    NoEligibleRoot,
    QuantifiedCommonNeedsAnalysis
};

struct RootPlanComponent {
    std::vector<std::size_t> nodes;
    std::optional<std::size_t> root;
    RootSelectionStatus status = RootSelectionStatus::NoEligibleRoot;
};

// Proposed roots only: this plan does not yet authorize a change in execution order.
// Node/edge IDs are local to this plan, independent of global transition IDs.
struct RootSelectionPlan {
    std::shared_ptr<const PatternAst> originalAst;
    std::vector<RootPlanNode> nodes;
    std::vector<RootPlanEdge> edges;
    std::vector<RootPlanComponent> components;
    bool hasQuantifiedCommon = false;
};

class RootSelectionPass {
public:
    RootSelectionPlan run(const std::shared_ptr<const PatternAst>& ast) const;
};
}
