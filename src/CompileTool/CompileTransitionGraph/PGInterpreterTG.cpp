#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "TransitionGraph/TransitionTrace.hpp"
#include "TransitionGraph/TransitionPort.hpp"
#include "CompileTool/Common/CommonKey.hpp"
#include <algorithm>
#include <functional>


namespace CorGIReg{

PGInterpreterTG::PGInterpreterTG(const std::string graphMatchExpr):mParser(graphMatchExpr){
    mActGroupe = 0;
}

std::shared_ptr<TGCompiled>  PGInterpreterTG::interpret(void){
    mActGroupe = 0;
    std::shared_ptr<AstNode<tGTokensTypes>> tree = mParser.parse();
    mRootSelectionPlan = RootSelectionPass{}.run(tree);
    mQuantifierIndices.clear();
    indexQuantifiers(tree);
    return visit(tree)->compile();
}

namespace {
using Ast = std::shared_ptr<AstNode<tGTokensTypes>>;

void collectPaths(const Ast& tree, std::vector<Ast>& paths) {
    if (tree->getType() == tGTokensTypes::SEP) {
        collectPaths(tree->getChilds()[0], paths);
        collectPaths(tree->getChilds()[1], paths);
    } else paths.push_back(tree);
}
// Units of a path in traversal order: PREC places its right operand first.
void flatten(const Ast& tree, std::vector<Ast>& units) {
    const auto type = tree->getType();
    if (type != tGTokensTypes::NEXT && type != tGTokensTypes::PREC) { units.push_back(tree); return; }
    const auto& children = tree->getChilds();
    flatten(children[type == tGTokensTypes::PREC], units);
    flatten(children[type == tGTokensTypes::NEXT], units);
}
bool any(const Ast& tree, const std::function<bool(const Ast&)>& predicate) {
    if (predicate(tree)) return true;
    for (const auto& child : tree->getChilds()) if (any(child, predicate)) return true;
    return false;
}
bool numbered(const Ast& tree) {
    return (tree->getType() == tGTokensTypes::NEXT || tree->getType() == tGTokensTypes::PREC) &&
           !tree->getValue().empty() && tree->getValue() != "*";
}
std::optional<std::string> anchorIdentity(const Ast& unit) {
    if (unit->getType() != tGTokensTypes::CKEY && unit->getType() != tGTokensTypes::SCKEY) return std::nullopt;
    return parseCommonKey(unit->getValue()).identity();
}
}

std::shared_ptr<TGCompiled> PGInterpreterTG::interpretRooted(void){
    mActGroupe = 0;
    const auto tree = mParser.parse();
    mRootSelectionPlan = RootSelectionPass{}.run(tree);
    mQuantifierIndices.clear();
    indexQuantifiers(tree);
    std::vector<Ast> paths;
    collectPaths(tree, paths);
    // Path index -> (component, root identity) for rerooted components.
    std::map<std::size_t, std::pair<std::size_t, std::string>> rooted;
    const auto& plan = mRootSelectionPlan;
    for (std::size_t c = 0; c < plan.components.size(); ++c) {
        const auto& component = plan.components[c];
        if (component.status != RootSelectionStatus::Selected || !component.root) continue;
        const auto& root = plan.nodes[*component.root];
        const auto identity = anchorIdentity(std::const_pointer_cast<AstNode<tGTokensTypes>>(root.occurrences.front()));
        if (!identity) continue;
        std::set<std::size_t> members;
        for (auto node : component.nodes) members.insert(plan.nodes[node].paths.begin(), plan.nodes[node].paths.end());
        bool eligible = std::includes(root.paths.begin(), root.paths.end(), members.begin(), members.end());
        // A boundary also holds when the successors were captured earlier in
        // the same walk, which splitting a path would change: keep path order.
        for (auto path : members)
            eligible = eligible && !any(paths[path], numbered) &&
                !any(paths[path], [](const Ast& node) { return node->getType() == tGTokensTypes::END; });
        if (!eligible) continue;
        for (auto path : members) rooted.emplace(path, std::make_pair(c, *identity));
    }
    std::shared_ptr<TransitionGraph> result;
    std::map<std::shared_ptr<State>, std::size_t> groups;
    auto add = [&](std::shared_ptr<TransitionGraph> part, std::optional<std::size_t> group) {
        if (group) groups.emplace(part->getStartStates().at(0), *group);
        result = result ? sepF(result, part) : part;
    };
    for (std::size_t p = 0; p < paths.size(); ++p) {
        auto found = rooted.find(p);
        if (found == rooted.end()) { add(visit(paths[p]), std::nullopt); continue; }
        std::vector<Ast> units;
        flatten(paths[p], units);
        std::size_t k = 0;
        while (anchorIdentity(units[k]) != found->second.second) ++k;
        if (k + 1 < units.size() || k == 0) {
            auto forward = visit(units[k]);
            for (std::size_t j = k + 1; j < units.size(); ++j) forward = nextF(forward, visit(units[j]));
            add(forward, found->second.first);
        }
        if (k > 0) {
            auto backward = reversed(units[k]);
            for (std::size_t j = k; j-- > 0;) backward = nextF(backward, reversed(units[j]));
            add(backward, found->second.first);
        }
    }
    auto compiled = result->compile();
    compiled->setStartGroups(std::move(groups));
    return compiled;
}

std::shared_ptr<TransitionGraph> PGInterpreterTG::reversed(std::shared_ptr<AstNode<tGTokensTypes>> AstTree){
    const auto type = AstTree->getType();
    const auto& children = AstTree->getChilds();
    if (type == tGTokensTypes::KEY || type == tGTokensTypes::CKEY || type == tGTokensTypes::SCKEY)
        return keyF(AstTree)->setTransitionChildToParent();
    if (type == tGTokensTypes::NEXT || type == tGTokensTypes::PREC) {
        const auto& first = children[type == tGTokensTypes::PREC];
        const auto& second = children[type == tGTokensTypes::NEXT];
        return nextF(reversed(second), reversed(first));
    }
    if (type == tGTokensTypes::QOM) return quantify(reversed(children[0]), mQuantifierIndices.at(AstTree.get()), '+');
    if (type == tGTokensTypes::QZM) return quantify(reversed(children[0]), mQuantifierIndices.at(AstTree.get()), '*');
    if (type == tGTokensTypes::QZO) return quantify(reversed(children[0]), mQuantifierIndices.at(AstTree.get()), '?');
    if (type == tGTokensTypes::LPAREN) {
        mActGroupe += 1;
        auto out = reversed(children[0]);
        mActGroupe -= 1;
        return out;
    }
    // A boundary only ends a path; it never precedes the root of that path.
    throw std::logic_error("Cannot reverse this path unit");
}

std::shared_ptr<TransitionGraph> PGInterpreterTG::visit(std::shared_ptr<AstNode<tGTokensTypes>> AstTree){

    std::vector<std::shared_ptr<AstNode<tGTokensTypes>>> nextAstNodes = AstTree->getChilds();

    if(AstTree->getType() == tGTokensTypes::SEP){
        return sepF(visit(nextAstNodes[0]),visit(nextAstNodes[1]));
    }else if(AstTree->getType() == tGTokensTypes::NEXT || AstTree->getType() == tGTokensTypes::PREC){
        auto left = visit(nextAstNodes[0]);
        auto right = visit(nextAstNodes[1]);
        if (AstTree->getType() == tGTokensTypes::PREC) std::swap(left, right);
        const auto value = AstTree->getValue();
        if (value.empty() || value == "*") return nextF(left, right);
        const auto port = static_cast<PortNumber>(std::stoull(value));
        auto append = [&](std::shared_ptr<TransitionGraph> graph, PortAction action) {
            auto previous = *graph->getValidStates().begin();
            auto end = std::make_shared<State>(true, false);
            end->setGroupe(mActGroupe); previous->unValid();
            auto step = std::make_shared<TransitionPort>(previous, end, action, port);
            step->updateWeak(); graph->addTransition(step);
            return graph;
        };
        auto previous = left->getStartStates().at(0);
        auto start = std::make_shared<State>(false, true);
        start->setGroupe(mActGroupe); previous->unStart();
        auto step = std::make_shared<TransitionPort>(start, previous, PortAction::Begin, port);
        step->updateWeak(); left->addTransition(step);
        return append(nextF(append(left, PortAction::Check), right), PortAction::End);
    }else if(AstTree->getType() == tGTokensTypes::QOM){
        return quantify(visit(nextAstNodes[0]), mQuantifierIndices.at(AstTree.get()), '+');
    }else if(AstTree->getType() == tGTokensTypes::QZM){
        return quantify(visit(nextAstNodes[0]), mQuantifierIndices.at(AstTree.get()), '*');
    }else if(AstTree->getType() == tGTokensTypes::QZO){
        return quantify(visit(nextAstNodes[0]), mQuantifierIndices.at(AstTree.get()), '?');
    }else if(AstTree->getType() == tGTokensTypes::KEY || AstTree->getType() == tGTokensTypes::CKEY || AstTree->getType() == tGTokensTypes::SCKEY){
        return keyF(AstTree);
    }else if (AstTree->getType() == tGTokensTypes::END){
        return endF(AstTree);
    }else if(AstTree->getType() == tGTokensTypes::LPAREN){
        mActGroupe += 1;
        std::shared_ptr<TransitionGraph> out = visit(nextAstNodes[0]);
        mActGroupe -= 1;
        return out;
    }else{
        throw std::logic_error("visit Bad token type" );
    }
}


std::shared_ptr<TransitionGraph> PGInterpreterTG::keyF(std::shared_ptr<AstNode<tGTokensTypes>> AstNode){
    /*leaf 
        K -> V
    */
    if (AstNode->getType() == tGTokensTypes::KEY){
        
        return std::make_shared<TransitionGraph>( TransitionTypes::UNIQUE,
                                                AstNode->getValue(), mParser.getQuery(),mActGroupe);

    }else if(AstNode->getType() == tGTokensTypes::CKEY){

        return std::make_shared<TransitionGraph>( TransitionTypes::COMMON,
                                                AstNode->getValue(), mParser.getQuery(),mActGroupe);

    }else if(AstNode->getType() == tGTokensTypes::SCKEY){
        return std::make_shared<TransitionGraph>( TransitionTypes::STRICTCOMMON,
                                                AstNode->getValue(), mParser.getQuery(),mActGroupe);
    }else{

        throw std::logic_error("keyF Bad in AST" );
    }
    return nullptr;
}

std::shared_ptr<TransitionGraph> PGInterpreterTG::endF(std::shared_ptr<AstNode<tGTokensTypes>> AstNode){
    /*
        Null
        S -> V
    */
    return std::make_shared<TransitionGraph>( TransitionTypes::NNULL,
                                            AstNode->getValue(), mParser.getQuery(),mActGroupe);
}

std::shared_ptr<TransitionGraph> PGInterpreterTG::sepF(std::shared_ptr<TransitionGraph> leftTG,std::shared_ptr<TransitionGraph> rightTG){

    /*
    The idea is to have an identifier for each query, which allows us to verify that in the end all queries have a solution
    */

   /*
    The problem with merging, particularly the merging of valid states,
    is that we lose the origin of the query, which invalidates the solutions. 
    Two solutions are then possible : 

    - The first is to ensure that a state has multiple queries
    we can maintain a link between the TG and the query

    - The second possibility is to replace the 2 query indexes with a new one
    
    */
    leftTG->incOriginFrom(rightTG);
    return leftTG->unionG(rightTG);
    //return leftTG->fusionG(rightTG);
}

std::shared_ptr<TransitionGraph> PGInterpreterTG::nextF(std::shared_ptr<TransitionGraph> leftTG,std::shared_ptr<TransitionGraph> rightTG){
    /*
        combine the 2 Graph
        all valid node of A are  merge with Start B, Start B is un Start
        update the relative reference  

           A          B
        SA -> VA + SB -> VB
           A    B
        SA -> q -> VB
    */
    return leftTG->mergeOneStartOneValid(rightTG);
}

std::shared_ptr<TransitionGraph> PGInterpreterTG::precF(std::shared_ptr<TransitionGraph> leftTG,std::shared_ptr<TransitionGraph> rightTG){
    /*
        combine the 2 Graph
        all valid node of A are  merge with Start B, Start B is un Start
        update the relative reference  

           A          B
        SA -> VA + SB -> VB
           B    A
        SB -x> q -x> VA
    */
    // For the PRECEDENCE operator (<-), the resulting graph should match
    // a predecessor relationship.  The left graph represents the node that
    // is preceded, while the right graph represents the preceding node.
    // We simply merge the valid state of the left graph with the start of
    // the right graph without flipping the transition directions.
    // This keeps the transitions directed from the preceding node to the
    // preceded node.
    return rightTG->mergeOneStartOneValid(leftTG);
}

void PGInterpreterTG::indexQuantifiers(const std::shared_ptr<AstNode<tGTokensTypes>>& node) {
    // Postorder follows lexical symbol order: inner postfixes precede outer ones.
    for (const auto& child : node->getChilds()) indexQuantifiers(child);
    const auto type = node->getType();
    if (type == tGTokensTypes::QOM || type == tGTokensTypes::QZM || type == tGTokensTypes::QZO)
        mQuantifierIndices.emplace(node.get(), mQuantifierIndices.size() + 1);
}

std::shared_ptr<TransitionGraph> PGInterpreterTG::quantify(
    std::shared_ptr<TransitionGraph> body, std::size_t index, char symbol) {
    const auto starts = body->getStartStates();
    const auto ends = body->getValidStates();
    if (starts.size() != 1 || ends.size() != 1)
        throw std::logic_error("Quantifier requires a single entry and exit");
    auto first = starts.front();
    auto last = *ends.begin();
    auto entry = std::make_shared<State>(false, true);
    auto completed = std::make_shared<State>(false, false);
    auto exit = std::make_shared<State>(true, false);
    for (const auto& state : {entry, completed, exit}) state->setGroupe(mActGroupe);
    first->unStart();
    last->unValid();
    auto trace = [&](auto from, auto to, TraceAction action) {
        auto edge = std::make_shared<TransitionTrace>(from, to, action, index, symbol);
        edge->updateWeak();
        body->addTransition(edge);
    };
    trace(entry, first, TraceAction::Begin);
    trace(last, completed, TraceAction::Iteration);
    trace(completed, exit, TraceAction::End);
    if (symbol != '?') {
        auto type = body->containCommonTransition() ? TransitionTypes::REF : TransitionTypes::EMPTY;
        body->addTransition(TransitionFactory::make(completed, first, type));
    }
    if (symbol != '+') trace(entry, exit, TraceAction::Skip);
    return body;
}

}
