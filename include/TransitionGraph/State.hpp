
#pragma once

#include <set>
#include <vector>
#include <stdexcept>
#include <memory>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include "TransitionGraph/Transition.hpp"
#include "TransitionGraph/RunTimeContext.hpp"

// TODO pourquoi avoir les transition et les parents dans le meme objet ???

namespace CorGIReg
{

class Transition;


/** @brief compare two weak pointer for making a set of weak pointer */
template <typename T>
struct lex_compare {
    bool operator() (const std::weak_ptr<T> &lhs, const std::weak_ptr<T> &rhs)const {
        auto lptr = lhs.lock(), rptr = rhs.lock();
        if (!rptr) return false; // nothing after expired pointer
        if (!lptr) return true;
        return lptr < rptr;
    }
};

class State
{
private:

    /**
     * @brief use to have unique id for FSM representation 
    */
    static std::size_t sID;
    /**
     * the unique id of the state (TODO quite useless)
    */
    const std::size_t mId;

    /**
     * @brief the Transition of the node
     * @details the Transition have a shared ref to the node so we use weak ref
    */
    std::set<std::weak_ptr<Transition>,lex_compare<Transition>> mTransitions;
    /**
     * @brief the parent of the node
    */
    std::set<std::weak_ptr<State>,lex_compare<State>> mParents;
    
    /** @brief the state can be a neutral a start or a valid state */
    bool mStart = false;
    bool mValid = false;

    /** @brief the id that store the origin of the state as a parte of a TransitionGraph
     * during the construction of the graph somme state can be merged in the same state 
     * we cumulate the origin of the states, that is useful for validation of the match werre 
     * all the origine muste be valid
    */
    std::set<std::size_t> mFromTransitionGraph = {0};

    /** @brief the id that store the group of the state 
     * that correspond to the sub emsenble of common node deffinition
     * use for groupe quantification  ex : (B#>(A#>B)+)*
     */
    std::size_t mGroupId = 0;

public:
    State(bool isAValid,bool isAStart);
    ~State() = default;


    void rmTransition(std::shared_ptr<Transition>);
    void addTransition(std::shared_ptr<Transition>);
  
    const std::set<std::weak_ptr<Transition>,lex_compare<Transition>>& getTransitions(void){
        return mTransitions;
    }

    void addParent(std::shared_ptr<State>);
    void rmParent(std::shared_ptr<State>);

    const std::set<std::weak_ptr<State>,lex_compare<State>>& getParentStates(void){
        return mParents;
    }

    inline std::size_t getGroup(void){
        return mGroupId;
    }

    inline void setGroupe(std::size_t id){
        mGroupId = id;
    }

    /**
     * @brief return true if the state is a valid state, false otherwise
    */
    inline bool isValid(void){
        return mValid;
    }

    /**
     * @brief return true if the state is a start state, false otherwise
    */
    inline bool isStart(void){
        return mStart;
    }
    inline void unValid(void){
        mValid =false;
    }
    inline void valid(void){
        mValid =true;
    }
    inline void unStart(void){
        mStart =false;
    }
    inline void start(void){
        mStart =true;
    }

    inline const std::set<std::size_t>& getOrigin(){
        return mFromTransitionGraph;
    }

    inline void appendOrigin(std::set<std::size_t> origins){
        mFromTransitionGraph.insert(origins.begin(),origins.end());
    }

    inline void incOrigin(std::size_t increment){
        std::set<size_t> incrementedSet;  // Set to hold incremented values
        for (auto& i : mFromTransitionGraph){
            incrementedSet.insert(i + increment);
        }
        mFromTransitionGraph = incrementedSet;
    }

    const std::vector<std::shared_ptr<RunTimeContext>> test( std::shared_ptr<RunTimeContext> context);

    std::string strRep(void) const;
};




} // namespace CorGIReg