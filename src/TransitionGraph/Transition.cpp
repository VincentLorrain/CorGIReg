#include "TransitionGraph/Transition.hpp"


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
    std::string out;
    out +=  mStateSource->strRep() ;
    out += "--"+ mDirSourceToDest ? ">" : "o";
    out +=  mStateDest->strRep();
    out += "\n";
    return out;
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
    if(opNode == nullptr){
        return {false,std::set<std::shared_ptr<NodeNN>>()};//none
    }
    // if the node have more than one parent or child it is not unique by definition
    if(NodeDictionary::at(mNodeTestKey)(opNode) && opNode->getChildren().size() <= 1  && opNode->getParents().size() <= 1){
        //the actual node is unique and pass the test so we set it as valid
        context->setValid(opNode,mNodeTestKey);
        // get the successor of the node depending on the direction
        std::set<std::shared_ptr<NodeNN>> nextNode;
        if(mDirSourceToDest){
            nextNode = opNode->getChildren();
        }else{
            std::set<std::shared_ptr<NodeNN>>tmp = opNode->getParents();
            nextNode = std::set<std::shared_ptr<NodeNN>>(tmp.begin(), tmp.end()) ;
        }
        return {true,nextNode} ;

    }else{
        //context->addRejectedNode(opNode);
        return {false,std::set<std::shared_ptr<NodeNN>>()};
    }
}

std::string TransitionUnique::strRep(void) const{
    std::string out;
    out += mStateSource->strRep();
    out += fmt::format("--{}|{}|",mDirSourceToDest ? ">" : "o", mNodeTestKey);
    out += mStateDest->strRep();
    out += "\n";
    return out;
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

    if(opNode == nullptr){
        return {false,std::set<std::shared_ptr<NodeNN>>()};//none
    }
    if(NodeDictionary::at(mNodeTestKey)(opNode)){
        context->setCommon(opNode,mCommonIdx);
        context->setValid(opNode,mNodeTestKey);


        std::set<std::shared_ptr<NodeNN>> nextNode;

        if(mDirSourceToDest){
            nextNode = opNode->getChildren();
        }else{
            std::set<std::shared_ptr<NodeNN>>tmp = opNode->getParents();
            nextNode = std::set<std::shared_ptr<NodeNN>>(tmp.begin(), tmp.end()) ;
        }
        
        std::set<std::shared_ptr<NodeNN>> childNode=opNode->getChildren();
        std::set<std::shared_ptr<NodeNN>>tmp = opNode->getParents();
        std::set<std::shared_ptr<NodeNN>> parentNode = std::set<std::shared_ptr<NodeNN>>(tmp.begin(), tmp.end()) ;

        //to be sure to valid all the branch (child and parent) of a common node
        // the logic is add the child and parent of the node to the rejected node
        // depending of the flag 
        std::set<std::shared_ptr<NodeNN>> nodeThatMustBeValid ={};
        if(mFlag == CommonMode::ALL || (mDirSourceToDest && mFlag == CommonMode::BACK) 
        || (!mDirSourceToDest && mFlag == CommonMode::FRONT)){
            nodeThatMustBeValid =  opNode->getParents();
        }

        std::set<std::shared_ptr<NodeNN>> children = opNode->getChildren();
        // Append the children vector to the end of the parents vector
        if(mFlag == CommonMode::ALL || (mDirSourceToDest && mFlag == CommonMode::FRONT) 
        || (!mDirSourceToDest && mFlag == CommonMode::BACK)){
            //nodeThatMustBeValid.insert(nodeThatMustBeValid.end(), childrenVector.begin(), childrenVector.end());
            nodeThatMustBeValid.insert(children.begin(), children.end());
        }
        
        //be sure that no null ptr on the node that rejected
        //nodeThatMustBeValid.erase(std::remove(nodeThatMustBeValid.begin(), nodeThatMustBeValid.end(), nullptr), nodeThatMustBeValid.end());
        nodeThatMustBeValid.erase(nullptr);
        
        // the connection of the common node depending on the flag are add to the rejected node
        if (children.size() > 1 || opNode->getParents().size() > 1){
            if (nodeThatMustBeValid.size() > 1){
                for (const auto& next : nodeThatMustBeValid){
                    context->addRejectedNode(next); 
                }
            }
        }
        // special case were the common node is in a sequence only one children and one parent
        // this common node is treated like unique node, we dont add is connection to the rejected node
        // because it will be in conflict with the sequence end must be explisit by using $
           
        return {true,nextNode};


    }else{
        //context->addRejectedNode(opNode);
        return  {false,std::set<std::shared_ptr<NodeNN>>()};
    }
}


std::string TransitionCommon::strRep(void) const{
    std::string out;
    out += mStateSource->strRep();
    out += fmt::format("--{}|{}{}|",mDirSourceToDest ? ">" : "o",mFlag== CommonMode::ALL ? "$" : "#",mNodeTestKey);
    out += mStateDest->strRep();
    out += "\n";
    return out;
}



/*
TransitionRef
*/
TransitionRef::TransitionRef(std::shared_ptr<State> source,std::shared_ptr<State> dest)
:Transition(source,dest,""),mAnchor(getAnchor(dest)){   

}


std::pair<std::size_t,int> TransitionRef::getAnchor(std::shared_ptr<State>& dest) const{
    for (const auto& edgeStart : dest->getTransitions()){
        if (auto sharedEdge = edgeStart.lock()) {
            const std::map<size_t, int> commonRef = sharedEdge->getRelative();
            //is not obligatoery the min distant 
            auto minPair = std::min_element(commonRef.begin(), commonRef.end(),
            [](const auto& p1, const auto& p2) {return std::abs(p1.second) < std::abs(p2.second); });
            if (minPair != commonRef.end()) {
                return *minPair;
            }
        }else{
            throw std::runtime_error("Error in TransitionRef bad weak ptr");
        }
    }
    return {0,0};
}

const TransitionResult TransitionRef::test(const std::shared_ptr<RunTimeContext>& context){
    //from all common node defined in the context we try to get the anchor
    std::shared_ptr<NodeNN> refNode = context->getCommonNodeFromIdx(mAnchor.first);
    if (refNode){
        //from the anchor we get the next node depending on the delta
        std::set<std::shared_ptr<NodeNN>> see;
        return {true,refNode->getNodeDelta(mAnchor.second,see)};
    }
    return  {false,std::set<std::shared_ptr<NodeNN>>()};
}
std::string TransitionRef::strRep(void) const{
    std::string out;
    out += mStateSource->strRep();
    out += fmt::format("-->|REF{} {}|", mAnchor.first,mAnchor.second);
    out += mStateDest->strRep();
    out += "\n";
    return out;
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
    if(opNode == nullptr){
        return {false,std::set<std::shared_ptr<NodeNN>>()};
    }
    return {true,std::set<std::shared_ptr<NodeNN>>({opNode})};//none
}

std::string TransitionEmpty::strRep(void) const{
    std::string out;
    out += mStateSource->strRep();
    out += "-->|Empty|";
    out += mStateDest->strRep();
    out += "\n";
    return out;
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
    return {false,std::set<std::shared_ptr<NodeNN>>()};
 }
std::string TransitionNone::strRep(void) const{
    std::string out;
    out += mStateSource->strRep();
    out += "-->|None|";
    out += mStateDest->strRep();
    out += "\n";
    return out;
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
    std::string out;
    out += mStateSource->strRep();
    out += "-->|Null|";
    out += mStateDest->strRep();
    out += "\n";
    return out;
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
    } else if (type == TransitionTypes::COMMON) {
        // TODO make common mode Strict input or output
        std::smatch m;
        std::regex commonRegex("\\s*([A-Za-z_0-9])+\\#([0-9]*)");
        if (std::regex_match(lexeme, m, commonRegex)) {
            std::string nodeConditionKey = m[1];
            std::string commonId =  m[2];
            size_t commonIdx = commonId.empty() ? 0 : std::stoi(commonId) + 1;
            std::string commonKey = nodeConditionKey + std::to_string(commonIdx);
        
            if(!NodeDictionary::hasFunction(nodeConditionKey)){
                //if the key is not linked to a condition 
                //by default, it is initialized by a edge that is always false
                auto transitionNone = std::make_shared<TransitionNone>(source, dest);
                transitionNone->updateWeak();
                return transitionNone;
                //throw std::invalid_argument("Bad Node Test " + nodeConditionKey );
            }
            auto transitionCommon = std::make_shared<TransitionCommon> (source, dest, nodeConditionKey, commonKey,CommonMode::NONE);
            transitionCommon->updateWeak();
            return transitionCommon;
        } else {
            throw std::invalid_argument("error lexemeCOMMON " + lexeme);
        }
    } else if (type == TransitionTypes::STRICTCOMMON){
        std::smatch m;
        std::regex commonRegex("\\s*([A-Za-z_0-9])+\\$([0-9]*)");
        
        if (std::regex_match(lexeme, m, commonRegex)) {
            std::string nodeConditionKey = m[1];
            std::string commonId =  m[2];
            size_t commonIdx = commonId.empty() ? 0 : std::stoi(commonId) + 1;
            std::string commonKey = nodeConditionKey + std::to_string(commonIdx);
        
            if(!NodeDictionary::hasFunction(nodeConditionKey)){
                //if the key is not linked to a condition 
                //by default, it is initialized by a edge that is always false
                auto transitionNone = std::make_shared<TransitionNone>(source, dest);
                transitionNone->updateWeak();
                return transitionNone;
                //throw std::invalid_argument("Bad Node Test " + nodeConditionKey );
            }
            auto transitionCommon = std::make_shared<TransitionCommon> (source, dest, nodeConditionKey, commonKey,CommonMode::ALL);
            transitionCommon->updateWeak();
            return transitionCommon;
        } else {
            throw std::invalid_argument("error lexeme STRICTCOMMON " + lexeme);
        }

    } else if (type == TransitionTypes::UNIQUE) {
        std::regex uniqueRegex("\\s*([A-Za-z_0-9]+)");
        
        std::smatch m;
        if (std::regex_match(lexeme, m, uniqueRegex)) {
            std::string nodeConditionKey = m[1];

            if(!NodeDictionary::hasFunction(nodeConditionKey)){

                //if the key is not linked to a condition 
                //by default, it is initialized by a edge that is always false
                auto transitionNone = std::make_shared<TransitionNone>(source, dest);
                transitionNone->updateWeak();
                return transitionNone;
                //throw std::invalid_argument("Bad Node Test " + nodeConditionKey );
            }
            auto transitionUnique = std::make_shared<TransitionUnique>(source, dest, nodeConditionKey);
            transitionUnique->updateWeak();
            return transitionUnique;

        } else {
            throw std::invalid_argument("error lexeme UNIQUE \"" + std::string(lexeme) +" eee\""); // TODO eee ?
        }
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