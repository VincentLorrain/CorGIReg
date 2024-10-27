#include "TransitionGraph/State.hpp"


namespace CorGIReg{

std::size_t State::sID = 0;



State::State(bool isAValid,bool isAStart ):mId(sID++){
    mValid =isAValid;
    mStart =isAStart;
}

void State::rmTransition(std::shared_ptr<Transition> transition){
    mTransitions.erase(transition);
}
void State::addTransition(std::shared_ptr<Transition> transition){
    std::weak_ptr<Transition> transitionW(transition);
    if (!transitionW.expired()) {
        mTransitions.insert(transitionW);
    }else{
        throw std::runtime_error("addTransition State weak pointer is expired" );
    }
}


void State::addParent(std::shared_ptr<State> node){

    std::weak_ptr<State> nodeW(node);
    if (!nodeW.expired()) {
        mParents.insert(nodeW);
    }else{
        throw std::runtime_error("addParent State weak pointer is expired" );
    }
}

void State::rmParent(std::shared_ptr<State> node){
    mParents.erase(node);
}

const std::vector<std::shared_ptr<RunTimeContext>> State::test( std::shared_ptr<RunTimeContext> context){


    std::vector<std::shared_ptr<RunTimeContext>> out;

    for(auto transition : mTransitions){
        if (std::shared_ptr<Transition> sharedTransition = transition.lock()) {
            const std::shared_ptr<State> nextState =  sharedTransition->getDestState();
            //make deep copy of the context
            std::shared_ptr<RunTimeContext> newContext = std::make_shared<RunTimeContext>(context);
            TransitionResult edgeRes = sharedTransition->test(newContext);
            
            if(edgeRes.success){
                if(edgeRes.node.size() != 0){
                    for(auto nextNode :edgeRes.node ){
                        if(!newContext->isAlreadyValid(nextNode)|| newContext->isCommonDefined(nextNode) ){
                            out.push_back( std::make_shared<RunTimeContext>(newContext,nextState,nextNode));
                           
                        }else{
                            out.push_back( std::make_shared<RunTimeContext>(newContext,nextState,nullptr));
                        }

                    }
                }else{
                    out.push_back( std::make_shared<RunTimeContext>(newContext,nextState,nullptr));
                }
            }
            newContext.reset();

        }else{
            throw std::runtime_error("test State weak pointer is expired" );
        }

    }
    return out;
}

std::string State::strRep(void) const{
    std::string out;
    if(!mStart && !mValid)
    {
        out += fmt::format("{}(({}_{}))",mId, mGroupId,  fmt::join(mFromTransitionGraph, ", "));
    }
    else if (!mStart && mValid)
    {
         out += fmt::format("{}([{}_{}])",mId, mGroupId,  fmt::join(mFromTransitionGraph, ", "));
    }
    else if (mStart && !mValid)
    {
         out += fmt::format("{}[{}_{}]",mId, mGroupId,  fmt::join(mFromTransitionGraph, ", "));
    }
    else if (mStart && mValid)
    {
         out += fmt::format("{} [{}_{}]",mId, mGroupId,  fmt::join(mFromTransitionGraph, ", "));
    }
   
    return out;
}

}