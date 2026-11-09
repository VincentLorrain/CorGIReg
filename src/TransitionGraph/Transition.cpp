#include "CompileTool/Common/CommonKey.hpp"
#include "TransitionGraph/Transition.hpp"
#include "GraphAccess/GraphTraversal.hpp"
#include <deque>
#include <tuple>


namespace CorGIReg{

std::atomic<std::size_t> Transition::sID{0};

/**
 * \section Transition general
 */

Transition::Transition(std::shared_ptr<State> source,std::shared_ptr<State> dest,std::string nodeTestKey)
:mNodeTestKey(nodeTestKey)
{
    mStateSource = source;
    mStateDest   = dest;
    
    // Endpoint links are registered by updateWeak once shared ownership exists.
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

    // Compare anchor metadata on each adjacent pair before propagating it.
    std::set<std::size_t> myRelativeID;
    for (const auto& kvp : mRelativeToCommon) {
        myRelativeID.insert(kvp.first);
    }

    for (const auto& nextWeakTransition : mStateDest->getTransitions()){
        
        if (auto nextTransition = nextWeakTransition.lock()) {
            // A self-loop has no separate neighbor to update.
            if(this == nextTransition.get()){
                continue;
            }
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

            // A shared legacy reference must differ by one step between
            // consecutive transitions.
            std::set<std::size_t> idxsTotest;
            for (auto idx : nextRelativeID){
                if (myRelativeID.find(idx) != myRelativeID.end()){
                    if (std::abs(getRelative().at(idx) - nextTransition->getRelative().at(idx)) != 1) {
                        throw std::runtime_error("Bad relative");
                    }
                }
            }
            
            // Propagate references that only this transition knows.
            std::map<size_t,int> tmpRelative;
            for(auto idxToPush :idxsToPush ){
                tmpRelative.insert( std::make_pair(idxToPush, getRelative().at(idxToPush) +1));
            }
            if(tmpRelative.size() != 0){
                nextTransition->updateRelative(tmpRelative);
                nextTransition->propagateRelativePos();
            }
            tmpRelative.clear();


            // Bring back references known only by the next transition.
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
TransitionRef::TransitionRef(std::shared_ptr<State> source, std::shared_ptr<State> dest)
    : Transition(source, dest, ""), mBody(analyzeBody(dest, source)) {}

TransitionRef::Body TransitionRef::analyzeBody(const std::shared_ptr<State>& start,
                                               const std::shared_ptr<State>& completed) {
    Body body;
    using Position = std::tuple<std::shared_ptr<State>, int, bool>;
    std::deque<Position> pending{{start, 0, false}};
    std::set<Position> visited;
    bool childDirected = false, parentDirected = false;
    while (!pending.empty()) {
        auto [state, delta, anchored] = pending.front();
        pending.pop_front();
        if (!visited.emplace(state, delta, anchored).second) continue;
        if (state == completed) {
            body.canSkipAnchors |= !anchored;
            continue;
        }
        for (const auto& edge : state->getTransitions()) {
            auto transition = edge.lock();
            if (!transition) throw std::runtime_error("Expired transition while finding common anchor");
            bool usesAnchor = anchored;
            switch (transition->getType()) {
            case TransitionTypes::COMMON:
            case TransitionTypes::STRICTCOMMON: {
                auto& anchor = body.anchors[transition->getCommonIdx()];
                if (delta < -kLimit) anchor.unboundedParents = true;
                else if (delta > kLimit) anchor.unboundedChildren = true;
                else anchor.deltas.insert(delta);
                usesAnchor = true;
                break;
            }
            // Do not follow another repetition's jump when locating its body.
            case TransitionTypes::REF:
            case TransitionTypes::NONE:
                continue;
            case TransitionTypes::EMPTY:
            case TransitionTypes::NNULL:
                pending.emplace_back(transition->getDestState(), delta, anchored);
                continue;
            case TransitionTypes::UNIQUE:
                break;
            }
            const bool children = transition->isDirectionIsChildren();
            childDirected |= children;
            parentDirected |= !children;
            // Keep following long paths so distant anchors are still found.
            const int next = std::abs(delta) > kLimit ? delta : delta + (children ? -1 : 1);
            pending.emplace_back(transition->getDestState(), next, usesAnchor);
        }
    }
    if (childDirected && parentDirected) {
        for (auto& entry : body.anchors) {
            auto& anchor = entry.second;
            if (anchor.unboundedParents || anchor.unboundedChildren)
                anchor.unboundedParents = anchor.unboundedChildren = true;
        }
    }
    return body;
}

const TransitionResult TransitionRef::test(const std::shared_ptr<RunTimeContext>& context) {
    // A skipped optional anchor has no binding. Other bound anchors can
    // still locate a start, and the body checks all bindings during the walk.
    const auto& graph = context->graph();
    NodeIds result;
    if (mBody.canSkipAnchors) {
        if (auto next = context->getActNode()) result.insert(*next);
    }
    for (const auto& [identity, anchor] : mBody.anchors) {
        const auto refNode = context->getCommonNodeFromIdx(identity);
        if (!refNode) continue;
        if (anchor.deltas.count(0)) result.insert(*refNode);
        for (const bool parents : {true, false}) {
            const bool unbounded = parents ? anchor.unboundedParents : anchor.unboundedChildren;
            int last = 0;
            for (int delta : anchor.deltas)
                if (parents ? delta < 0 : delta > 0) last = std::max(last, std::abs(delta));
            NodeIds layer{*refNode}, seen;
            for (int step = 1; !layer.empty() && (unbounded || step <= last); ++step) {
                NodeIds next;
                for (NodeId node : layer)
                    for (NodeId neighbor : parents ? graph.parents(node) : graph.children(node)) next.insert(neighbor);
                if (unbounded) {
                    for (auto it = next.begin(); it != next.end();)
                        it = seen.insert(*it).second ? std::next(it) : next.erase(it);
                    result.insert(next.begin(), next.end());
                } else if (anchor.deltas.count(parents ? -step : step)) {
                    result.insert(next.begin(), next.end());
                }
                layer = std::move(next);
            }
        }
    }
    return {!result.empty() || mBody.canSkipAnchors, std::move(result)};
}

std::string TransitionRef::strRep(void) const {
    std::stringstream ss;
    ss << mStateSource->getId() << "-->|REF";
    for (const auto& [identity, anchor] : mBody.anchors)
        ss << fmt::format(" {} [{}{}{}]", identity, fmt::join(anchor.deltas, ","),
                          anchor.unboundedParents ? " -inf" : "", anchor.unboundedChildren ? " +inf" : "");
    if (mBody.canSkipAnchors) ss << " or next";
    ss << '|' << mStateDest->getId();
    return ss.str();
}

TransitionEmpty::TransitionEmpty(std::shared_ptr<State> source, std::shared_ptr<State> dest)
    : Transition(source, dest, "") {}

const TransitionResult TransitionEmpty::test(const std::shared_ptr<RunTimeContext>& context) {
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

// A boundary tests the cursor after unavailable successors have been removed.
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
