
#pragma once

#include <atomic>
#include <set>
#include <vector>
#include <stdexcept>
#include <memory>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include "TransitionGraph/Transition.hpp"
#include "TransitionGraph/RunTimeContext.hpp"

namespace CorGIReg
{

class Transition;


/** @brief Order live graph objects by creation, independently of allocation. */
template <typename T>
struct lex_compare {
    bool operator() (const std::weak_ptr<T> &lhs, const std::weak_ptr<T> &rhs)const {
        auto lptr = lhs.lock(), rptr = rhs.lock();
        if (!rptr) return false; // nothing after expired pointer
        if (!lptr) return true;
        return lptr->getId() < rptr->getId();
    }
};

class State
{
private:

    // Allocate unique creation IDs independently of memory addresses.
    static std::atomic<std::size_t> sID;
    // Creation order breaks ties when the compiler or matcher visits states.
    const std::size_t mId;

    // Transitions own their endpoint states. Weak back-links avoid ownership
    // cycles and let a state enumerate its outgoing transitions.
    std::set<std::weak_ptr<Transition>,lex_compare<Transition>> mTransitions;
    // Incoming state links support compiler updates when transitions are rewired.
    std::set<std::weak_ptr<State>,lex_compare<State>> mParents;
    
    // Independent start and accepting flags. An intermediate state has neither.
    bool mStart = false;
    bool mValid = false;

    // Origins identify the compiled paths a walk can satisfy. A complete
    // solution must cover every origin in the machine.
    std::set<std::size_t> mFromTransitionGraph = {0};

    // The compiler uses the group level to guard merges across nested regions.
    // This is separate from the start groups used to enumerate candidates.
    std::size_t mGroupId = 0;

public:
    State(bool isAValid,bool isAStart);
    ~State() = default;
    // A state is identified by its id and its place in the graph: a copy would
    // duplicate the id and refer to transitions that do not point back to it.
    State(const State&) = delete;
    State& operator=(const State&) = delete;


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
     * @brief return the id of the state
     * @return the id of the state
    */
    inline std::size_t getId(void){
        return mId;
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
