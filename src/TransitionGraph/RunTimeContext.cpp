#include "TransitionGraph/RunTimeContext.hpp"

namespace CorGIReg {
    

std::vector<std::set<std::shared_ptr<NodeNN>>> RunTimeContext::mRejectedNodes;

RunTimeContext::RunTimeContext(std::shared_ptr<State> actState ,std::shared_ptr<NodeNN> actOpNode ,std::size_t idxRejected  ){
    mActOpNode = actOpNode;
    mActState  = actState;

    //not define case
    if(idxRejected ==  std::numeric_limits<std::size_t>::max()){
        mLocalIdxRejected =  mRejectedNodes.size();
        mRejectedNodes.push_back(std::set<std::shared_ptr<NodeNN>>());
    }else{
        if(idxRejected > mRejectedNodes.size()-1 ){
            throw std::runtime_error("RunTimeContext idxRejected");
        }
        mLocalIdxRejected =idxRejected;
    }
}

RunTimeContext::RunTimeContext(std::shared_ptr<RunTimeContext> fsmRunTime){
    mActOpNode        = fsmRunTime->mActOpNode;
    mActState         = fsmRunTime->mActState;
    mCommonNodes      = fsmRunTime->mCommonNodes;
    mValidNodes       = fsmRunTime->mValidNodes;
    mLocalIdxRejected  = fsmRunTime->mLocalIdxRejected;
}

RunTimeContext::RunTimeContext(std::shared_ptr<RunTimeContext> fsmRunTime,std::shared_ptr<State> actState ,std::shared_ptr<NodeNN> actOpNode ){
    mActOpNode        = actOpNode;
    mActState         = actState;
    mCommonNodes      = fsmRunTime->mCommonNodes;
    mValidNodes       = fsmRunTime->mValidNodes;
    mLocalIdxRejected  = fsmRunTime->mLocalIdxRejected;
}


void RunTimeContext::setCommon(std::shared_ptr<NodeNN> node,std::size_t commonIdx){
    if(isCommonDefined(node)){
        if (mCommonNodes.at(node) != commonIdx){
            throw std::runtime_error("conflict idx in the Common node");
        }
    }else{
        mCommonNodes[node] = commonIdx;
    }
}


std::set<std::shared_ptr<NodeNN>> RunTimeContext::getCommonNodes(void) const{
    std::set<std::shared_ptr<NodeNN>> nodes;
    // Iterate over the map and insert values into the set
    for (const auto& pair : mCommonNodes) {
        nodes.insert(pair.first);
    }
    return nodes;
}


std::set<std::shared_ptr<NodeNN>> RunTimeContext::getValidNodesNoCommon(void) const{
    std::set<std::shared_ptr<NodeNN>> differenceSet;
    std::set<std::shared_ptr<NodeNN>> valid = getValidNodes();
    std::set<std::shared_ptr<NodeNN>> common = getCommonNodes();
    std::set_difference(valid.begin(), valid.end(), common.begin(), common.end(),std::inserter(differenceSet, differenceSet.end()));
    return differenceSet;
}

std::shared_ptr<NodeNN> RunTimeContext::getCommonNodeFromIdx(const std::size_t& commonIdx){
    for (const auto& pair : mCommonNodes) {
        if (pair.second == commonIdx) {
            return pair.first; // Return the key when the value is found
        }
    }
    return nullptr;
}

bool RunTimeContext::isCommonDefined(const std::shared_ptr<NodeNN>& node) {
    const auto& commonNodes = getCommonNodes();
    return commonNodes.find(node) != commonNodes.end();
}


std::set<std::shared_ptr<NodeNN>> RunTimeContext::getValidNodes(void) const{

    auto sharedSet = std::make_shared<std::set<std::shared_ptr<NodeNN>>>();
    // Create a set to store the values from the map
    std::set<std::shared_ptr<NodeNN> > nodes;
    // Iterate over the map and insert values into the set
    for (const auto& pair : mValidNodes) {
        nodes.insert(pair.second.begin(),pair.second.end());
    }
    return nodes;
}


bool RunTimeContext::isAlreadyValid(std::shared_ptr<NodeNN>  node){

    std::set<std::shared_ptr<NodeNN>> nodes = getValidNodes();
    for(const auto& nodeV : nodes){
        if(nodeV.get() == node.get()){
            return true;
        }
    }
    return false;

    //return getValidNodes().find(node) != getValidNodes().end();
}

bool RunTimeContext::isOnValidState(void){
    return mActState->isValid();
}

 void RunTimeContext::setValid(std::shared_ptr<NodeNN>  node, const std::string tag){
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

     }


bool RunTimeContext::areEqual(const std::shared_ptr<RunTimeContext>& context) const{
    /*
    A Context is equal all the evaluation are the same
    */
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
    /*
    Context are compatible if all the Common node are the same
    the valid node are the different 
     */

   for (const auto& ref : mCommonNodes) {
        for (const auto& test : context->mCommonNodes) {
            //same index
            if(ref.second == test.second){
                if(ref.first != test.first){
                    return false;
                }
            }
        }
   }

   //valid nodes
    std::set<std::shared_ptr<NodeNN>> commonElements;
    std::set<std::shared_ptr<NodeNN>> A = getValidNodesNoCommon();
    std::set<std::shared_ptr<NodeNN>> B = context->getValidNodesNoCommon();
    std::set_intersection(
        A.begin(),A.end(),
        B.begin(),  B.end(),
        std::inserter(commonElements, commonElements.end())
       );

    return (commonElements.empty()) ? true : false;
}


std::set<std::size_t>  RunTimeContext::getSubStmId(void) const {
    return mActState->getOrigin();
}


std::shared_ptr<State> RunTimeContext::getActState(){
    return mActState;
}

}


