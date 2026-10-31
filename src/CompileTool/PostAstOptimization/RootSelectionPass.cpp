#include "CompileTool/Common/CommonKey.hpp"
#include "CompileTool/PostAstOptimization/RootSelectionPass.hpp"
#include <algorithm>
#include <map>
#include <queue>
#include <stdexcept>

namespace CorGIReg {
namespace {
using Type = tGTokensTypes;
using Ast = std::shared_ptr<const PatternAst>;

bool common(Type type) { return type == Type::CKEY || type == Type::SCKEY; }
bool quantified(Type type) { return type == Type::QOM || type == Type::QZM || type == Type::QZO; }

bool containsCommon(const Ast& node) {
    if (common(node->getType())) return true;
    for (const auto& child : node->getChilds()) if (containsCommon(child)) return true;
    return false;
}

// # and $ constrain the same anchor identity; strictness stays on each occurrence.
// An omitted suffix differs from explicit 0; numeric suffixes ignore leading zeros.
std::pair<std::string, std::string> commonIdentity(const Ast& node) {
    const auto key = parseCommonKey(node->getValue());
    return {parseNodePattern(key.predicate).name, key.suffix};
}

class Builder {
    RootSelectionPlan plan;
    std::map<std::pair<std::string, std::string>, std::size_t> anchors;
    std::set<std::pair<std::size_t, std::size_t>> edges;
    std::size_t pathCount = 0;

    void requireArity(const Ast& node, std::size_t count) {
        if (node->nbChild() != count) throw std::invalid_argument("Invalid AST arity in root selection");
    }

    // Quantified regions are opaque units. This phase must not unroll or move them.
    std::pair<std::size_t, std::size_t> sequence(const Ast& node, std::size_t path) {
        const auto type = node->getType();
        if (type == Type::NEXT || type == Type::PREC) {
            requireArity(node, 2);
            const auto left = sequence(node->getChilds()[0], path);
            const auto right = sequence(node->getChilds()[1], path);
            if (type == Type::NEXT) {
                edges.emplace(left.second, right.first);
                return {left.first, right.second};
            }
            // Matches precF: the entire right sequence precedes the left sequence.
            edges.emplace(right.second, left.first);
            return {right.first, left.second};
        }
        if (quantified(type)) {
            requireArity(node, 1);
            plan.hasQuantifiedCommon |= containsCommon(node);
        } else if (type == Type::KEY || common(type) || type == Type::END) {
            requireArity(node, 0);
        } else {
            throw std::invalid_argument("Unsupported AST unit in root selection");
        }
        std::size_t id = plan.nodes.size();
        if (common(type)) {
            auto found = anchors.find(commonIdentity(node));
            if (found != anchors.end()) id = found->second;
            else anchors.emplace(commonIdentity(node), id);
        }
        if (id == plan.nodes.size())
            plan.nodes.push_back({{}, {}, type == Type::KEY || common(type)});
        plan.nodes[id].occurrences.push_back(node);
        plan.nodes[id].paths.insert(path);
        return {id, id};
    }

    void paths(const Ast& node) {
        if (node->getType() == Type::SEP) {
            requireArity(node, 2);
            paths(node->getChilds()[0]);
            paths(node->getChilds()[1]);
        } else sequence(node, pathCount++);
    }

public:
    RootSelectionPlan run(const Ast& ast) {
        if (!ast) throw std::invalid_argument("Root selection requires an AST");
        plan.originalAst = ast;
        paths(ast);
        std::vector<std::set<std::size_t>> adjacent(plan.nodes.size());
        for (auto [source, target] : edges) {
            plan.edges.push_back({source, target});
            if (source != target) {
                adjacent[source].insert(target);
                adjacent[target].insert(source);
            }
        }
        std::vector<bool> visited(plan.nodes.size(), false);
        for (std::size_t first = 0; first < plan.nodes.size(); ++first) {
            if (visited[first]) continue;
            RootPlanComponent component;
            std::queue<std::size_t> pending;
            pending.push(first);
            visited[first] = true;
            while (!pending.empty()) {
                auto current = pending.front(); pending.pop();
                component.nodes.push_back(current);
                for (auto next : adjacent[current]) if (!visited[next]) {
                    visited[next] = true;
                    pending.push(next);
                }
            }
            std::sort(component.nodes.begin(), component.nodes.end());
            // A common anchor inside repetition may join otherwise separate units,
            // with iteration-dependent identity. Do not claim independent components.
            if (plan.hasQuantifiedCommon) {
                component.status = RootSelectionStatus::QuantifiedCommonNeedsAnalysis;
            } else {
                for (auto id : component.nodes) {
                    if (!plan.nodes[id].rootEligible) continue;
                    auto score = [&](std::size_t candidate) {
                        return std::make_pair(plan.nodes[candidate].paths.size(), adjacent[candidate].size());
                    };
                    if (!component.root || score(id) > score(*component.root)) component.root = id;
                }
                if (component.root) component.status = RootSelectionStatus::Selected;
            }
            plan.components.push_back(std::move(component));
        }
        return std::move(plan);
    }
};
}

RootSelectionPlan RootSelectionPass::run(const std::shared_ptr<const PatternAst>& ast) const {
    return Builder{}.run(ast);
}
}
