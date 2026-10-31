#include "CompileTool/Common/CommonKey.hpp"
#include "TransitionGraph/Transition.hpp"
#include "GraphAccess/GraphTraversal.hpp"
#include <deque>


namespace CorGIReg{

/**
 * \section Transition general
 */

Transition::Transition(std::shared_ptr<State> source,std::shared_ptr<State> dest,std::string nodeTestKey)
:mNodeTestKey(nodeTestKey)
{
    mStateSource = source;
    mStateDest   = dest;
    
    // wen i make the Transition I init the nodes
    // mStateSource->addTransition(shared_from_this());
    // mStateDest->addParent(mStateSource);
}

void Transition::updateWeak(void){
    mStateSource->addTransition(shared_from_this());
    mStateDest->addParent(mStateSource);
}

std::string Transition::strRep(void) const{
    std::stringstream ss;
    ss <<  mStateSource->getId() ;
    ss << "--"+ mDirSourceToDest ? ">" : "o";
    ss <<  mStateDest->getId();
    return ss.str();
}

void Transition::reSetDestState(const std::shared_ptr<State> newDest){
        mStateDest->rmParent(mStateSource);
        mStateDest = newDest;
        mStateDest->addParent(mStateSource);
        propagateRelativePos();
}

void Transition::reSetSourceState(const std::shared_ptr<State> newSource){
    mStateSource->rmTransition(shared_from_this());
    mStateSource = newSource;
    mStateSource->addTransition(shared_from_this());
    propagateRelativePos();

}


void Transition::propagateRelativePos(void){

    // Get the id of common node were the transition is relative to
    std::set<std::size_t> myRelativeID;
    for (const auto& kvp : mRelativeToCommon) {
        myRelativeID.insert(kvp.first);
    }

    // run through all the transition recursively
    for (const auto& nextWeakTransition : mStateDest->getTransitions()){
        
        if (auto nextTransition = nextWeakTransition.lock()) {
            //if we get the actual Transition we skip it 
            if(this == nextTransition.get()){
                continue;
            }
            //if we get a others Transition we look at all the common node reference
            std::set<std::size_t> nextRelativeID;
            for (const auto& kvp : nextTransition->getRelative()) {
                nextRelativeID.insert(kvp.first);
            }

            // Find elements in myRelativeID but not in nextRelativeID
            std::set<std::size_t> idxsToPush;
            std::set_difference(myRelativeID.begin(), myRelativeID.end(),
                                nextRelativeID.begin(), nextRelativeID.end(),
                                std::inserter(idxsToPush, idxsToPush.begin()));

            // Find elements in nextRelativeID but not in myRelativeID
            std::set<std::size_t> idxsToGet;
            std::set_difference(nextRelativeID.begin(), nextRelativeID.end(),
                                myRelativeID.begin(), myRelativeID.end(),
                                std::inserter(idxsToGet, idxsToGet.begin()));

            //  test for integrity we look if 2 Transition refer to the same
            //  ref and are link the ref dif is one
            //  not working for common node
            //  we can go deeper by find the all pass to a ref and see if the delta is good

            // Find elements present in both myRelativeID and nextRelativeID
            std::set<std::size_t> idxsTotest;
            for (auto idx : nextRelativeID){
                if (myRelativeID.find(idx) != myRelativeID.end()){
                    if (std::abs(getRelative().at(idx) - nextTransition->getRelative().at(idx)) != 1) {
                        throw std::runtime_error("Bad relative");
                    }
                }
            }
            
            // this Transition have more relative info than the next
            std::map<size_t,int> tmpRelative;
            // we push this info to the next 
            for(auto idxToPush :idxsToPush ){
                tmpRelative.insert( std::make_pair(idxToPush, getRelative().at(idxToPush) +1));
            }
            if(tmpRelative.size() != 0){
                nextTransition->updateRelative(tmpRelative);
                nextTransition->propagateRelativePos();
            }
            tmpRelative.clear();


            // the next node have more info than me i need to get it
            for(auto idxToGet :idxsToGet ){
                tmpRelative.insert( std::make_pair(idxToGet, nextTransition->getRelative().at(idxToGet) -1));
            }
            if(tmpRelative.size() != 0){
                updateRelative(tmpRelative);
                
                for(auto weakParent : getSourceState()->getParentStates()){
                    if (auto parent = weakParent.lock()) {
                        for(auto weakPTransition : parent->getTransitions()){
                            if (auto pTransition = weakPTransition.lock()) {
                                pTransition->propagateRelativePos();
                            }else{
                                throw std::runtime_error("propagateRelativePos parent Transition weak pointer is expired" );
                            }
                        }
                    }else{
                        throw std::runtime_error("propagateRelativePos parent weak pointer is expired" );
                    }
                }
            }
            tmpRelative.clear();
        }else{
            throw std::runtime_error("propagateRelativePos Transition weak pointer is expired" );
        }
    }
}


void Transition::updateRelative( const std::map<size_t,int>& relative ){
    for (const auto& kvp : relative) {
            mRelativeToCommon.insert(kvp);
    }
}

/**
 * \section Transition Unique
 */

TransitionUnique::TransitionUnique(std::shared_ptr<State> source,std::shared_ptr<State> dest,const std::string nodeTestKey)
:Transition(source,dest,nodeTestKey){}

const TransitionResult TransitionUnique::test(const std::shared_ptr<RunTimeContext>& context){
    //get the actual node of the graph 
    auto opNode = context->getActNode();
    if(!opNode){
        return {false,std::set<NodeId>()};//none
    }
    // A plain occurrence is a node of its own: it never re-captures a node of
    // this walk, even one an anchor allows the walk to revisit.
    if (context->isAlreadyValid(*opNode)) return {false,std::set<NodeId>()};
    // if the node have more than one parent or child it is not unique by definition
    if(context->testNode(mNodeTestKey, *opNode) && context->graph().children(*opNode).size() <= 1  && context->graph().parents(*opNode).size() <= 1){
        //the actual node is unique and pass the test so we set it as valid
        context->setValid(*opNode,mNodeTestKey);
        // get the successor of the node depending on the direction
        std::set<NodeId> nextNode;
        if(mDirSourceToDest){
            nextNode = context->graph().children(*opNode);
        }else{
            std::set<NodeId>tmp = context->graph().parents(*opNode);
            nextNode = std::set<NodeId>(tmp.begin(), tmp.end()) ;
        }
        return {true,nextNode} ;

    }else{
        //context->addRejectedNode(opNode);
        return {false,std::set<NodeId>()};
    }
}

std::string TransitionUnique::strRep(void) const{
    std::stringstream ss;
    ss << mStateSource->getId();
    ss << fmt::format("--{}|{}|",mDirSourceToDest ? ">" : "o", mNodeTestKey);
    ss << mStateDest->getId();
    return ss.str();
}

/*
TransitionCommon 
*/
std::map<std::string,std::size_t> TransitionCommon::mCommonIdxMap;

TransitionCommon::TransitionCommon(std::shared_ptr<State> source,std::shared_ptr<State> dest, const std::string nodeTestKey, 
const std::string commonKey,CommonMode flag)
:Transition(source,dest,nodeTestKey)
{
    mFlag = flag;
    //make a uid for common node 
    if(mCommonIdxMap.find(commonKey) == mCommonIdxMap.end()){
        mCommonIdxMap.insert(std::make_pair(commonKey, mCommonIdxMap.size()));
    }
    mCommonIdx = mCommonIdxMap[commonKey];
    propagateRelativePos();
}
const TransitionResult TransitionCommon::test(const std::shared_ptr<RunTimeContext>& context){
    
    auto opNode = context->getActNode();

    if(!opNode){
        return {false,std::set<NodeId>()};//none
    }
    if(context->testNode(mNodeTestKey, *opNode)){
        // A node holds one common identity and an identity binds one node:
        // A#1 then A#2 on the same node, or one anchor on two nodes, rejects
        // this walk instead of aborting the search.
        const auto bound = context->getCommonNodeFromIdx(mCommonIdx);
        if (bound ? *bound != *opNode : context->isCommonDefined(*opNode))
            return {false,std::set<NodeId>()};
        context->setCommon(*opNode,mCommonIdx);
        context->setValid(*opNode,mNodeTestKey);


        std::set<NodeId> nextNode;

        if(mDirSourceToDest){
            nextNode = context->graph().children(*opNode);
        }else{
            std::set<NodeId>tmp = context->graph().parents(*opNode);
            nextNode = std::set<NodeId>(tmp.begin(), tmp.end()) ;
        }
        
        // Strict coverage is an obligation of this occurrence, including degree 1.
        // INPUTS/OUTPUTS are absolute; legacy FRONT/BACK remain traversal-relative.
        const bool inputs = mFlag == CommonMode::ALL || mFlag == CommonMode::INPUTS ||
            (mDirSourceToDest && mFlag == CommonMode::BACK) ||
            (!mDirSourceToDest && mFlag == CommonMode::FRONT);
        const bool outputs = mFlag == CommonMode::ALL || mFlag == CommonMode::OUTPUTS ||
            (mDirSourceToDest && mFlag == CommonMode::FRONT) ||
            (!mDirSourceToDest && mFlag == CommonMode::BACK);
        if (inputs) for (auto node : context->graph().parents(*opNode)) context->addRejectedNode(node);
        if (outputs) for (auto node : context->graph().children(*opNode)) context->addRejectedNode(node);

        return {true,nextNode};


    }else{
        //context->addRejectedNode(opNode);
        return  {false,std::set<NodeId>()};
    }
}


std::string TransitionCommon::strRep(void) const{
    std::stringstream ss;
    ss << mStateSource->getId();
    const bool inputs = mFlag == CommonMode::ALL || mFlag == CommonMode::INPUTS ||
        (mDirSourceToDest && mFlag == CommonMode::BACK) || (!mDirSourceToDest && mFlag == CommonMode::FRONT);
    const bool outputs = mFlag == CommonMode::ALL || mFlag == CommonMode::OUTPUTS ||
        (mDirSourceToDest && mFlag == CommonMode::FRONT) || (!mDirSourceToDest && mFlag == CommonMode::BACK);
    ss << fmt::format("--{}|{}{}{}|", mDirSourceToDest ? ">" : "o", inputs ? "$" : "",
                      mNodeTestKey, outputs ? "$" : inputs ? "" : "#");
    ss << mStateDest->getId();
    return ss.str();
}



/*
TransitionRef
*/
TransitionRef::TransitionRef(std::shared_ptr<State> source,std::shared_ptr<State> dest)
:Transition(source,dest,""),mAnchor(getAnchor(dest)){   

}


TransitionRef::Anchor TransitionRef::getAnchor(std::shared_ptr<State>& dest) const{
    // Find an actual common transition in the repeated body. Falling back to
    // global ID zero made matches depend on which query was compiled first.
    // Optional or repeated operands before the anchor give several distances:
    // (A?->D#)+ starts its next iteration at D or at a parent of D.
    Anchor anchor;
    std::optional<std::size_t> commonIdx;
    std::deque<std::pair<std::shared_ptr<State>, int>> pending{{dest, 0}};
    std::set<std::pair<std::shared_ptr<State>, int>> visited;
    while (!pending.empty()) {
        auto [state, delta] = pending.front();
        pending.pop_front();
        if (!visited.insert({state, delta}).second) continue;
        for (const auto& edge : state->getTransitions()) {
            auto transition = edge.lock();
            if (!transition) throw std::runtime_error("Expired transition while finding common anchor");
            switch (transition->getType()) {
            case TransitionTypes::COMMON:
            case TransitionTypes::STRICTCOMMON:
                if (!commonIdx) commonIdx = transition->getCommonIdx();
                if (*commonIdx == transition->getCommonIdx()) anchor.deltas.insert(delta);
                continue;
            // Do not follow another repetition's jump when locating its body.
            case TransitionTypes::REF:
                continue;
            case TransitionTypes::EMPTY:
                pending.push_front({transition->getDestState(), delta});
                continue;
            case TransitionTypes::UNIQUE:
            case TransitionTypes::NNULL:
            case TransitionTypes::NONE:
                break;
            }
            const int next = delta + (transition->isDirectionIsChildren() ? -1 : 1);
            if (next < -kLimit) anchor.unboundedParents = true;
            else if (next > kLimit) anchor.unboundedChildren = true;
            else pending.push_back({transition->getDestState(), next});
        }
    }
    if (!commonIdx) throw std::runtime_error("No common anchor reachable in repeated body");
    anchor.commonIdx = *commonIdx;
    return anchor;
}

const TransitionResult TransitionRef::test(const std::shared_ptr<RunTimeContext>& context){
    //from all common node defined in the context we try to get the anchor
    std::optional<NodeId> refNode = context->getCommonNodeFromIdx(mAnchor.commonIdx);
    if (!refNode) return {false,std::set<NodeId>()};
    // Candidate starts of the next iteration. Distances count walk steps; the
    // repeated body then checks each candidate up to the same anchor.
    const auto& graph = context->graph();
    NodeIds result;
    if (mAnchor.deltas.count(0)) result.insert(*refNode);
    for (const bool parents : {true, false}) {
        const bool unbounded = parents ? mAnchor.unboundedParents : mAnchor.unboundedChildren;
        int last = 0;
        for (int delta : mAnchor.deltas) if (parents ? delta < 0 : delta > 0) last = std::max(last, std::abs(delta));
        NodeIds layer{*refNode}, seen;
        for (int step = 1; !layer.empty() && (unbounded || step <= last); ++step) {
            NodeIds next;
            for (NodeId node : layer)
                for (NodeId neighbor : parents ? graph.parents(node) : graph.children(node)) next.insert(neighbor);
            if (unbounded) {
                // Every node on an unbounded side is a candidate.
                for (auto it = next.begin(); it != next.end();) it = seen.insert(*it).second ? std::next(it) : next.erase(it);
                result.insert(next.begin(), next.end());
            } else if (mAnchor.deltas.count(parents ? -step : step)) {
                result.insert(next.begin(), next.end());
            }
            layer = std::move(next);
        }
    }
    return {true, result};
}
std::string TransitionRef::strRep(void) const{
    std::stringstream ss;
    ss << mStateSource->getId();
    ss << fmt::format("-->|REF{} {}{}{}|", mAnchor.commonIdx, fmt::join(mAnchor.deltas, ","),
                      mAnchor.unboundedParents ? " -inf" : "", mAnchor.unboundedChildren ? " +inf" : "");
    ss << mStateDest->getId();
    return ss.str();
}
/*
TransitionEmpty
*/
TransitionEmpty::TransitionEmpty(std::shared_ptr<State> source,std::shared_ptr<State> dest)
:Transition(source,dest,"")
{}
const TransitionResult TransitionEmpty::test(const std::shared_ptr<RunTimeContext>& context){
    /**
     * accept all type of nodes if there existing in the graph
     */
    auto opNode = context->getActNode();
    if(!opNode){
        return {false,std::set<NodeId>()};
    }
    return {true,std::set<NodeId>({*opNode})};//none
}

std::string TransitionEmpty::strRep(void) const{
    std::stringstream ss;
    ss << mStateSource->getId();
    ss << "-->|Empty|";
    ss << mStateDest->getId();
    return ss.str();
}

/*
TransitionNone
*/
TransitionNone::TransitionNone(std::shared_ptr<State> source,std::shared_ptr<State> dest)
:Transition(source,dest,"")
{}
 const TransitionResult TransitionNone::test(const std::shared_ptr<RunTimeContext>& /*context*/){
    /**
     * refuse all the node, can not build the transition 
     */
    return {false,std::set<NodeId>()};
 }
std::string TransitionNone::strRep(void) const{
    std::stringstream ss;
    ss << mStateSource->getId();
    ss << "-->|None|";
    ss << mStateDest->getId();
    return ss.str();
}

/*
TransitionNull
*/
TransitionNull::TransitionNull(std::shared_ptr<State> source,std::shared_ptr<State> dest)
:Transition(source,dest,""){
}

// TODO check if the context can be pass as ref ??
const TransitionResult TransitionNull::test(const std::shared_ptr<RunTimeContext>& context) {
    const auto current_node = context->getActNode();
    if (!current_node) {
        return {true, {}};
    }
    return {false, {}};
}

std::string TransitionNull::strRep(void) const{
    std::stringstream ss;
    ss << mStateSource->getId();
    ss << "-->|Null|";
    ss << mStateDest->getId();
    return ss.str();
}

/*

*/

 std::shared_ptr<Transition> TransitionFactory::make(std::shared_ptr<State> source,std::shared_ptr<State> dest,
TransitionTypes type,const std::string lexeme)
{
    //default transition

    if (type == TransitionTypes::EMPTY) {
        if (lexeme.empty()) {
            auto transitionEmpty = std::make_shared<TransitionEmpty>(source, dest);
            transitionEmpty->updateWeak();
            return transitionEmpty;
        } else {
            throw std::invalid_argument("error lexeme EMPTY");
        }
    } else if (type == TransitionTypes::REF) {
        if (lexeme.empty()) {
            auto transition = std::make_shared<TransitionRef>(source, dest);
            transition->updateWeak();
            return transition;
        } else {
            throw std::invalid_argument("error lexeme REF ");
        }
    } else if (type == TransitionTypes::COMMON || type == TransitionTypes::STRICTCOMMON) {
        const auto key = parseCommonKey(lexeme);
        const bool strict = key.strictInputs || key.strictOutputs;
        if (strict != (type == TransitionTypes::STRICTCOMMON))
            throw std::invalid_argument("Common transition type disagrees with node spelling: " + lexeme);
        const auto mode = key.strictInputs && key.strictOutputs ? CommonMode::ALL :
                          key.strictInputs ? CommonMode::INPUTS :
                          key.strictOutputs ? CommonMode::OUTPUTS : CommonMode::NONE;
        auto transition = std::make_shared<TransitionCommon>(source, dest, key.predicate, key.identity(), mode);
        transition->updateWeak();
        return transition;
    } else if (type == TransitionTypes::UNIQUE) {
        const auto node = parseNodePattern(lexeme);
        if (node.anchor != node.name) throw std::invalid_argument("Invalid unique node: " + lexeme);
        auto transition = std::make_shared<TransitionUnique>(source, dest, node.predicate);
        transition->updateWeak();
        return transition;
    }else if (type == TransitionTypes::NNULL){
        if (lexeme.empty()) {
            auto transitionNull = std::make_shared<TransitionNull>(source, dest);
            transitionNull->updateWeak();
            return transitionNull;
        } else {
            throw std::invalid_argument("error lexeme EMPTY");
        }
    
    } else {
        throw std::invalid_argument("Bad edge Type");
    }
    return nullptr;
}


} // namespace CorGIReg
