
#include "TransitionGraph/TransitionGraph.hpp"

namespace CorGIReg{

TGCompiled::TGCompiled(std::set<std::size_t> origins,std::set<std::shared_ptr<Transition>> transitions,CommonMode,const std::string query):mQuery(query){

    mAllOrigin = origins;
    mTransitions = transitions;
}

const std::set<std::shared_ptr<State>> TGCompiled::getStates(void){
    std::set<std::shared_ptr<State>> states;
    for(auto transition : mTransitions){
        states.insert(transition->getDestState());
        states.insert(transition->getSourceState());
    }
    return states;

}

const std::vector<std::shared_ptr<State>> TGCompiled::getStartStates(void){
    std::set<std::shared_ptr<State>> states = getStates();
    std::vector<std::shared_ptr<State>> startStates;
    for(auto state :states){
        if(state->isStart()){
            startStates.push_back(state);
        }
    }
    return startStates;

}
const std::set<std::shared_ptr<State>> TGCompiled::getValidStates(void){
    std::set<std::shared_ptr<State>> states = getStates();
    std::set<std::shared_ptr<State>> ValidStates;
    for(auto state :states){
        if(state->isValid()){
            ValidStates.insert(state);
        }
    }
    return ValidStates;
}
std::size_t TGCompiled::getNbStart(void){
    return getStartStates().size();
}




std::shared_ptr<SolutionFromStartingPoint> TGCompiled::test(const std::vector<std::shared_ptr<NodeNN>>& startNodes){
    /*
    create the initial walks with the start nodes on the graph NN and the start States on the TransitionGraph
    */
    std::vector<std::shared_ptr<State>> startNodesFsm = getStartStates();
    if(startNodes.size() != startNodesFsm.size()){
         throw std::runtime_error("bad number of Start nodes");
    }

    std::vector<std::shared_ptr<RunTimeContext>> walks;
    for(std::size_t i = 0; i < startNodes.size(); i++){
        walks.push_back(std::make_shared<RunTimeContext>(startNodesFsm[i],startNodes[i]));
    }

    std::vector<std::shared_ptr<RunTimeContext>> nextWalks;
    std::vector<std::shared_ptr<RunTimeContext>> allValidContext;
    std::vector<std::shared_ptr<RunTimeContext>> allContextSee;

    while (!walks.empty())
    {
        for(auto actContext : walks){
            allContextSee.push_back(actContext);
            //if we are in a valid st we save it
            //it's one solution of the possible solution of the matching
            if(actContext->isOnValidState()){
                //not save 2 time the same end point
                if(!std::any_of(allValidContext.begin(), allValidContext.end(),
                    [&](std::shared_ptr<RunTimeContext> oldValid) {
                        return actContext->areEqual(oldValid);
                })){
                    allValidContext.push_back(actContext);
                }

            }

            //dont test 2 time a actContext
            std::vector<std::shared_ptr<RunTimeContext>> tmpNextWalks = actContext->getActState()->test(actContext);
            for(auto PotentialFsmContext : tmpNextWalks){

                if(!std::any_of(allContextSee.begin(), allContextSee.end(),
                    [&](std::shared_ptr<RunTimeContext> oldSee) {
                        return PotentialFsmContext->areEqual(oldSee);
                })){
                    nextWalks.push_back(PotentialFsmContext);
                }
            }

        }
        walks.swap(nextWalks);
        nextWalks.clear();
    }
    /*
    at this point we have all the valid context, but a match is a set a valid context from starting nodes
    */


    
    return std::make_shared<SolutionFromStartingPoint>(allValidContext,mAllOrigin,mQuery,startNodes);

    // MatchResult allMatch(allValidContext,getNbSubFsm(),mQuery,startNodes);
    // RunTimeContext::rst();
    // return allMatch.getSolutions();

}

/********************************************************** */

// TransitionGraph::TransitionGraph(const std::string query, CommonMode flag ):mQuery(query){
//     mFlag = flag;
// }

// TransitionGraph::TransitionGraph(const std::string query):mQuery(query){
// }



 TransitionGraph::TransitionGraph( TransitionTypes type,
                                const std::string lexeme,
                                const std::string query,
                                std::size_t groupeIdx):mQuery(query){

    std::shared_ptr<State>  start = std::make_shared<State>(false,true);
    std::shared_ptr<State>  valid = std::make_shared<State>(true,false);

    auto transition = TransitionFactory::make(start,valid,type,lexeme);
    addTransition(transition);
    /*must be set after addTransition*/
    setGroupe(groupeIdx);
}



bool TransitionGraph::containCommonTransition(void){

    for(auto transition : mTransitions){
        if(transition->isCommon()){
             return true; 
        }
    }
    return false;
}

std::shared_ptr<TransitionGraph> TransitionGraph::addTransitionStartToValid(TransitionTypes type,
                                                            const std::string lexeme){

    std::vector<std::shared_ptr<State>> allStart =  getStartStates();
    std::set<std::shared_ptr<State>> allValid = getValidStates();

    if(allStart.empty() || allValid.empty()){   
        return shared_from_this();
    }
    
    if(allStart.size() != 1){
         //throw std::logic_error("addTransitionStartToValid error" );
         return nullptr;
    }

    for(auto start : allStart){
        for(auto valid : allValid){
            auto transition = TransitionFactory::make(start,valid,type,lexeme);
            addTransition(transition);
        }
    } 

    return shared_from_this();
}

std::shared_ptr<TransitionGraph> TransitionGraph::addTransitionValidToStart(TransitionTypes type,
                                                            const std::string lexeme){
    
    std::vector<std::shared_ptr<State>> allStart =  getStartStates();
    std::set<std::shared_ptr<State>> allValid = getValidStates();

  
    if(allStart.size() != 1){
         //throw std::logic_error("addTransitionStartToValid error" );
         return nullptr;
    }

    for(auto start : allStart){
        for(auto valid : allValid){
            auto transition = TransitionFactory::make(valid,start,type,lexeme);
            addTransition(transition);
        }
    } 
    return shared_from_this();
}

void TransitionGraph::addTransition(std::shared_ptr<Transition> transition){
    
    //mAllOrigin.insert(transition->getDestState()->getOrigin());
    mAllOrigin.insert(transition->getDestState()->getOrigin().begin(), 
                  transition->getDestState()->getOrigin().end());

    //mAllOrigin.insert(transition->getSourceState()->getOrigin());
    mAllOrigin.insert(transition->getSourceState()->getOrigin().begin(), 
                  transition->getSourceState()->getOrigin().end());

    mTransitions.insert(transition);
}

void TransitionGraph::setGroupe(std::size_t groupeIdx){
    std::set<std::shared_ptr<State>> nodes = getStates();
    for(auto node :nodes){
        node->setGroupe(groupeIdx);
    }
}

std::shared_ptr<TransitionGraph> TransitionGraph::setTransitionChildToParent(void){
      for(auto transition : mTransitions){
        if (transition->isDirectionIsChildren()){
            transition->flipDir();
        }
    }
    return shared_from_this();
}

std::shared_ptr<TransitionGraph> TransitionGraph::setTransitionParentToChild(void){
      for(auto transition : mTransitions){
        if (!transition->isDirectionIsChildren()){
            transition->flipDir();
        }
    }
    return shared_from_this();
}

std::shared_ptr<TransitionGraph> TransitionGraph::unionG(const std::shared_ptr<TransitionGraph> transitionGraph){
  

    /*
    union all the transition the origin of the 2 TransitionGraph will also be merged
    */
    for(auto transition : transitionGraph->getTransitions()){
        addTransition(transition);
    }
    return shared_from_this();
}

std::shared_ptr<TransitionGraph> TransitionGraph::fusionG(const std::shared_ptr<TransitionGraph> transitionGraph){
    /**
     * 
     */
     /*
    we add all the node of the fsmGraph to this , and we can optimize using common transition 
    the common edge are by definition the same edges in the 2 fsm graph (warning the common of the same context )
    so if 2 state (node of the FSM) of the 2 fsm have the same input transition of a common node these 2 state can be merge; 
    1) find the mergeable sate in the 2 fsm
    2) append the FSM
    3) merge the sate 
    */


   /**
    * get all the FSM node (stat) that have has input a Common Edge in the two graph
    * then  
   */
    std::vector<std::pair<std::shared_ptr<State>,std::shared_ptr<State>>> statesPairsCompat;

    for(auto transitionInput : transitionGraph->getTransitions()){
        if (std::shared_ptr<TransitionCommon> targetEdge = std::dynamic_pointer_cast<TransitionCommon>(transitionInput)) {
            for(auto transitionSelf : getTransitions()){
                if (std::shared_ptr<TransitionCommon> actualEdge = std::dynamic_pointer_cast<TransitionCommon>(transitionSelf)) {
                    //we get 2 edges of the 2 fsm that test the same condition 
                    if( targetEdge->getTestKey() == actualEdge->getTestKey() ){

                        std::shared_ptr<State> targetState = targetEdge->getDestState();
                        std::shared_ptr<State> actualState = actualEdge->getDestState();
                        /*filter test*/
                        if (actualState->getParentStates().size() == 1 && targetState->getParentStates().size() == 1){
                            statesPairsCompat.push_back({targetState, actualState});
                        }
                        
                    }
                }
            }
        }
    }
    //add all the node to the FSM ,
    unionG(transitionGraph);

    for (const auto& pair : statesPairsCompat) {
        _mergeState(pair.first,pair.second);
    }

    return shared_from_this();
}

std::shared_ptr<TransitionGraph> TransitionGraph::mergeOneStartOneValid(const std::shared_ptr< TransitionGraph> transitionGraph){

    std::set<std::shared_ptr<State>> validStates = getValidStates();
    std::vector<std::shared_ptr<State>> startStates = transitionGraph->getStartStates();

    if (startStates.size() != 1 || validStates.size() != 1){

        // std::ostringstream errorMessage;
        // errorMessage <<"mergeOneStartOneValid  start size: " << startStates.size() << " valid size : " << validStates.size()
        // <<" can only merge FSM 1 start 1 valid";
        // throw std::runtime_error(errorMessage.str());
        //throw std::runtime_error("mergeOneStartOneValid");
        return nullptr;
    }

    unionG(transitionGraph);
    //get the start and valid nodes and merge them
    //the loop is not really necessary because the size of validStates and startStates are 1
    //but it future version we will potentially have more than 1 start and 1 valid
    for(auto valid : validStates){
        valid->unValid();
        for(auto start : startStates){
            start->unStart();
            _mergeState(start,valid);
        }
    }

    return shared_from_this();
}

std::shared_ptr<TransitionGraph> TransitionGraph::incOriginFrom(const std::shared_ptr< TransitionGraph> transitionGraph){
    
    /*
    before union we need to set the origin to be sure that have no intersection between the 2 TransitionGraph
    */
    auto inputOrigin = transitionGraph->getAllOrigin();
    std::size_t maxIn = 0;
    std::size_t maxSelf = 0;

    if(!inputOrigin.empty())
        maxIn=*inputOrigin.rbegin();

    if(!mAllOrigin.empty())
        maxSelf=*mAllOrigin.rbegin();

    /*
    shift by max of the origin to be sure of no overlap origin 
    */
    if(maxIn >= maxSelf){
        transitionGraph->incOriginBy(maxSelf+1);
    }else{
        incOriginBy(maxIn+1);
    }

    return shared_from_this();
}

void TransitionGraph::incOriginBy(const std::size_t incr){

    std::set<std::shared_ptr<State>> states = getStates();
    for(auto state :states){
        state->incOrigin(incr);
    }
    std::set<std::size_t> updatedOrigin;
    for(auto origin : mAllOrigin){
        updatedOrigin.insert(origin + incr);
    }
    mAllOrigin.swap(updatedOrigin);
}

std::shared_ptr<TGCompiled> TransitionGraph::compile(void) const{
    return std::make_shared<TGCompiled>(mAllOrigin,mTransitions,mFlag,mQuery);
}


void TransitionGraph::_mergeState(std::shared_ptr<State> source,std::shared_ptr<State> dest){

    /*
    combine the 2 state is combine the origin propagate the valid and start attribute
    and reforme transistion
    */

    /*ASSERT*/

    const std::set<std::shared_ptr<State>> states = getStates();
    if(states.find(source) == states.end() || states.find(dest) == states.end()){
        throw std::runtime_error("TransitionGraph can not merge node not in the graph");
    }

    if(source->getGroup() != dest->getGroup()){
        throw std::runtime_error("Can we merge 2 states of diferente groupe , i think it is not possible");
    }

    if(!std::includes(mAllOrigin.begin(),mAllOrigin.end(),source->getOrigin().begin(),source->getOrigin().end()) || 
    !std::includes(mAllOrigin.begin(),mAllOrigin.end(),dest->getOrigin().begin(),dest->getOrigin().end())){
        throw std::runtime_error("TransitionGraph can not merge node not in the graph mAllOrigin");
    }

    /*END*/

    /*
    the origin of the source are concat with the origin of the dest
    */
    std::set<std::size_t> originDest = dest->getOrigin();
    std::set<std::size_t> originSource = source->getOrigin();

    dest->appendOrigin(source->getOrigin());

    bool mergeValidState = false;

    //propagate source attribute
    if(source->isValid()){
        dest->valid();
    }
    if(source->isStart()){
        dest->start();
    }


    //merge source to dest by replace source by dest in all EDGE
    for(auto edge : mTransitions){
        if(edge->getDestState() == source ){
            edge->reSetDestState(dest);
        }else if(edge->getSourceState() == source ){
            edge->reSetSourceState(dest);
        }
    }

    /*ASSERT*/
    //check is source is not longer in the graph
    const std::set<std::shared_ptr<State>> nodesEnd = getStates();
   
    if(nodesEnd.find(source) !=  nodesEnd.end() ){

        throw std::runtime_error("FsmGraph merge node not effective");
    }

}

/***********************************************/

const std::vector<std::shared_ptr<State>> TransitionGraph::getStartStates(void){
    std::set<std::shared_ptr<State>> states = getStates();
    std::vector<std::shared_ptr<State>> startStates;
    for(auto state :states){
        if(state->isStart()){
            startStates.push_back(state);
        }
    }
    return startStates;

}
const std::set<std::shared_ptr<State>> TransitionGraph::getValidStates(void){
    std::set<std::shared_ptr<State>> states = getStates();
    std::set<std::shared_ptr<State>> ValidStates;
    for(auto state :states){
        if(state->isValid()){
            ValidStates.insert(state);
        }
    }
    //may short
    return ValidStates;

}
const std::set<std::shared_ptr<State>> TransitionGraph::getStates(void){
    std::set<std::shared_ptr<State>> states;
    for(auto transition : mTransitions){
        states.insert(transition->getDestState());
        states.insert(transition->getSourceState());
    }
    return states;

}

}