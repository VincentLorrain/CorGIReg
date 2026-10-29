#include "TransitionGraph/RunTimeContext.hpp"
#include "TransitionGraph/State.hpp"

namespace CorGIReg {
void RunTimeContext::beginQuantifier(std::size_t index, char symbol) {
    std::optional<std::size_t> parent;
    if (!mActiveQuantifiers.empty()) parent = mActiveQuantifiers.back().first;
    mQuantifiers.push_back({index, symbol, parent, mCaptures.size(), mCaptures.size(), {}});
    mActiveQuantifiers.push_back({mQuantifiers.size() - 1, getValidNodes().size()});
}

bool RunTimeContext::completeIteration(std::size_t index) {
    auto& active = mActiveQuantifiers.at(mActiveQuantifiers.size() - 1);
    auto& capture = mQuantifiers.at(active.first);
    if (capture.index != index) throw std::logic_error("Unbalanced quantifier iteration");
    const auto size = getValidNodes().size();
    // Canonical finite traces: another iteration must cover a new source node.
    // This also bounds nullable and common-node-only repetition cycles.
    if (capture.count() != 0 && size == active.second) return false;
    active.second = size;
    capture.iterationEnds.push_back(mCaptures.size());
    return true;
}

void RunTimeContext::endQuantifier(std::size_t index) {
    auto& capture = mQuantifiers.at(mActiveQuantifiers.at(mActiveQuantifiers.size() - 1).first);
    if (capture.index != index) throw std::logic_error("Unbalanced quantifier exit");
    capture.end = mCaptures.size();
    mActiveQuantifiers.pop_back();
}
    

RunTimeContext::RunTimeContext(std::shared_ptr<State> state, std::optional<NodeId> node,
                               std::shared_ptr<const MatchEnvironment> environment)
    : mEnvironment(std::move(environment)),
      mActState(std::move(state)), mActOpNode(node) {}
RunTimeContext::RunTimeContext(std::shared_ptr<RunTimeContext> context)
    : RunTimeContext(*context) {}
RunTimeContext::RunTimeContext(std::shared_ptr<RunTimeContext> context,
                               std::shared_ptr<State> state, std::optional<NodeId> node)
    : RunTimeContext(*context) {
    mActState = std::move(state);
    mActOpNode = node;
}

void RunTimeContext::setCommon(NodeId node,std::size_t commonIdx){
    if(isCommonDefined(node)){
        if (mCommonNodes.at(node) != commonIdx){
            throw std::runtime_error("conflict idx in the Common node");
        }
    }else{
        mCommonNodes[node] = commonIdx;
    }
}


std::set<NodeId> RunTimeContext::getCommonNodes(void) const{
    std::set<NodeId> nodes;
    // Iterate over the map and insert values into the set
    for (const auto& pair : mCommonNodes) {
        nodes.insert(pair.first);
    }
    return nodes;
}


std::set<NodeId> RunTimeContext::getValidNodesNoCommon(void) const{
    std::set<NodeId> differenceSet;
    std::set<NodeId> valid = getValidNodes();
    std::set<NodeId> common = getCommonNodes();
    std::set_difference(valid.begin(), valid.end(), common.begin(), common.end(),std::inserter(differenceSet, differenceSet.end()));
    return differenceSet;
}

std::optional<NodeId> RunTimeContext::getCommonNodeFromIdx(const std::size_t& commonIdx){
    for (const auto& pair : mCommonNodes) {
        if (pair.second == commonIdx) {
            return pair.first; // Return the key when the value is found
        }
    }
    return std::nullopt;
}

bool RunTimeContext::isCommonDefined(NodeId node) {
    const auto& commonNodes = getCommonNodes();
    return commonNodes.find(node) != commonNodes.end();
}


std::set<NodeId> RunTimeContext::getValidNodes(void) const{

    // Create a set to store the values from the map
    std::set<NodeId > nodes;
    // Iterate over the map and insert values into the set
    for (const auto& pair : mValidNodes) {
        nodes.insert(pair.second.begin(),pair.second.end());
    }
    return nodes;
}


bool RunTimeContext::isAlreadyValid(NodeId  node){

    std::set<NodeId> nodes = getValidNodes();
    for(const auto& nodeV : nodes){
        if(nodeV == node){
            return true;
        }
    }
    return false;

    //return getValidNodes().find(node) != getValidNodes().end();
}

bool RunTimeContext::isOnValidState(void){
    return mActState->isValid();
}

 void RunTimeContext::setValid(NodeId  node, const std::string tag){
        //we already find a node of this type
        if(mValidNodes.find(tag) != mValidNodes.end()){
            /*only the common node can be valid more than one*/
            if(isAlreadyValid(node) && !isCommonDefined(node) ){
                throw std::runtime_error("setValid you valid tow time");
            }
            mValidNodes[tag].insert(node);
        }else{
            mValidNodes[tag] = {node};
        }
        mCaptures.push_back({node, tag});

     }


bool RunTimeContext::areEqual(const std::shared_ptr<RunTimeContext>& context) const{
    /*
    A Context is equal all the evaluation are the same
    */
    if (mEnvironment != context->mEnvironment) return false;
    if (mRejectedNodes != context->mRejectedNodes) return false;
    if (mPortScopes != context->mPortScopes) return false;
    if (!mPortScopes.empty() && mCaptures != context->mCaptures) return false;
    if ((!mQuantifiers.empty() && mCaptures != context->mCaptures) || mQuantifiers != context->mQuantifiers ||
        mActiveQuantifiers != context->mActiveQuantifiers) return false;
    if(mActOpNode != context->mActOpNode){
        return false;
    }
    if (mActState != context->mActState){
        return false;
    }
    if (mValidNodes != context->mValidNodes){
        return false;
    }
    if (mCommonNodes != context->mCommonNodes){
        return false;
    }

    return true;
}

bool RunTimeContext::areCompatible(const std::shared_ptr<RunTimeContext>& context) const{
    // Walks agree on anchors: an identity binds one node and a node holds one
    // identity. A plain capture is its own node: no other walk captures it.
    if (mEnvironment != context->mEnvironment) return false;
    for (const auto& ref : mCommonNodes)
        for (const auto& test : context->mCommonNodes)
            if ((ref.second == test.second) != (ref.first == test.first)) return false;
    auto disjoint = [](const std::set<NodeId>& plain, const std::set<NodeId>& all) {
        for (auto node : plain) if (all.count(node)) return false;
        return true;
    };
    return disjoint(getValidNodesNoCommon(), context->getValidNodes()) &&
           disjoint(context->getValidNodesNoCommon(), getValidNodes());
}


std::set<std::size_t>  RunTimeContext::getSubStmId(void) const {
    return mActState->getOrigin();
}


std::shared_ptr<State> RunTimeContext::getActState(){
    return mActState;
}

}


