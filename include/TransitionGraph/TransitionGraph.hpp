#pragma once

#include <set>
#include <map>
#include <string>
#include <algorithm>

#include <vector>
#include <memory>
#include <stdexcept> //error

#include "TransitionGraph/Transition.hpp"
#include "TransitionGraph/MatchSolution.hpp"
#include "BasicGraphIr/Graph.hpp"
#include "NodeTest/NodeCondition.hpp"

#include "BasicGraphIr/Graph.hpp"

namespace CorGIReg{

class TGCompiled
{
private:
    std::set<std::size_t> mAllOrigin;
    std::set<std::shared_ptr<Transition>> mTransitions;
    const std::string mQuery;
    CommonMode mFlag = CommonMode::NONE;

public:

    // TODO the query is useful ??

    TGCompiled(std::set<std::size_t> origins,std::set<std::shared_ptr<Transition>> transitions,CommonMode,const std::string query);
    ~TGCompiled() =default;
    // TODO
    std::shared_ptr<SolutionFromStartingPoint> test(const std::vector<std::shared_ptr<NodeNN>>& StartNodes);
    
    /**
     * @brief get the number of start state
     * @return number of start state
    */
    std::size_t getNbStart(void);

    /**
     * @brief get the set of all the node in the graph
     * @return set of all nodes
    */
    const std::set<std::shared_ptr<State>> getStates(void);
    /**
     * @brief get the list of the starting states
     * @details we need to use a vector because the order of the nodes is important for start node initialization \ref test()
    */
    const std::vector<std::shared_ptr<State>> getStartStates(void);
    /**
     * @brief get the set of the valid states
     * @return set of valid state
    */
    const std::set<std::shared_ptr<State>> getValidStates(void);
    /**
     * @brief get the set of the edge of the graph
     */
    inline const std::set<std::shared_ptr<Transition>>& getTransitions(void){
        return mTransitions;
    };
    
    inline const std::set<std::size_t>& getAllOrigin(void){
        return mAllOrigin;
    }

    /**
     * @brief export a mermaid of the FSM
     * @param path  path of the mermaid
    */
    void save(const std::string& path) const ;
};



class TransitionGraph: public std::enable_shared_from_this<TransitionGraph> 
{
private:
    /**
     * @brief the Transition Graph is consist of Transition, and it is made by combine Transition Graph
     * the origin are all the sub Transition Graph that have been combine in this Transition Graph
    */
    std::set<std::size_t> mAllOrigin;
    /**
     * @brief the set of all Transition in the graph (edges)
     */
    std::set<std::shared_ptr<Transition>> mTransitions;
    /**
     * @brief the query that graph is supposed to match
     */
    const std::string mQuery;
    /**
     * @brief Mode of the common mode
     * @todo is it necessary? the common mode mode can be set in the query for all the node
     */
    CommonMode mFlag = CommonMode::NONE;

public:

    // TransitionGraph(const std::string query);
    // TransitionGraph(const std::string query, CommonMode flag );

    /**
     * @brief Construct a new Transition Graph object make the simplest transition graph possible 
     * that a StartState and a EndState end one transition
     */
    TransitionGraph( TransitionTypes type,
    const std::string lexeme, const std::string query, std::size_t groupeIdx);


    virtual ~TransitionGraph() = default;


    std::shared_ptr<TransitionGraph> addTransitionStartToValid(TransitionTypes type,
                                    const std::string lexeme);

    std::shared_ptr<TransitionGraph> addTransitionValidToStart(TransitionTypes type,
                                    const std::string lexeme);



    /**
     * @brief set all transition to the child to parent direction
    */
     std::shared_ptr<TransitionGraph> setTransitionChildToParent(void);
    /**
     * @brief set all transition to the parent to child direction
     */
    std::shared_ptr<TransitionGraph> setTransitionParentToChild(void);
    /**
     * @brief make the union between this graph and an input graph append all the node of 
     * transitionGraph in the actual graph
     * @param transitionGraph graph to union
    */
    std::shared_ptr<TransitionGraph> unionG(const std::shared_ptr<TransitionGraph> transitionGraph);

    /**
     * @brief like unionG and fusion the common edge in the result graph
     * @param transitionGraph graph to fusion
    */
    std::shared_ptr<TransitionGraph> fusionG(const std::shared_ptr<TransitionGraph> transitionGraph);

    /**
     * @brief make the union between this graph and an input graph and merge the valid state to the start state
     * @param transitionGraph graph to merge
    */
     std::shared_ptr<TransitionGraph> mergeOneStartOneValid(const std::shared_ptr< TransitionGraph> transitionGraph);


    /**
     * @brief increment the origin of all States in the graph by the one of the given transition graph
     * @param transitionGraph graph that give the origin to increment
     * @return the modified graph
    */
    std::shared_ptr<TransitionGraph> incOriginFrom(const std::shared_ptr< TransitionGraph> transitionGraph);

   /**
     * @brief export a mermaid of the FSM
     * @param path  path of the mermaid
    */
    void save(const std::string& path) const ;
    /**
     * @brief get the compiled version of the graph that can be used in runtime
     * 
     */
    std::shared_ptr<TGCompiled> compile(void) const;

    /**
     * @brief check if the graph contain common transition
     * @return true if contain common transition
     */
    bool containCommonTransition(void);

    //private:

    /**
     * @brief set a groupe (not the origin) idx for all the nodes in the graph
    */
    void setGroupe(std::size_t groupeIdx);

    /**
     * @brief increment the origin of all States in the graph
    */
    void incOriginBy(const std::size_t incr);

    /**
     * @brief merge two node of the graph
     * @param node
    */
    void _mergeState(std::shared_ptr<State> source,std::shared_ptr<State> dest);

    /**
     * @brief add edge in the graph, as Transition know the source and dest State these nodes are also add to the graph
    */
    void addTransition(std::shared_ptr<Transition> transition);


    /*
    get set +-
    */




    inline const std::set<std::size_t>& getAllOrigin(void){
        return mAllOrigin;
    }
    /**
     * @brief test the dir of graph true if node->child , false if node->Parent 
    */
    bool isGraphDirectionIsChildren(void);

    /**
     * @brief get the list of the starting states
     * @details we need to use a vector because the order of the nodes is important for start node initialization \ref test()
    */
    const std::vector<std::shared_ptr<State>> getStartStates(void);
    /**
     * @brief get the set of the valid states
     * @return set of valide state
    */
    const std::set<std::shared_ptr<State>> getValidStates(void);
    /**
     * @brief get the set of the edge of the graph
     */
    inline const std::set<std::shared_ptr<Transition>>& getTransitions(void){
        return mTransitions;
    }
    /**
     * @brief get the set of all the node in the graph
     * @return set of all nodes
    */
    const std::set<std::shared_ptr<State>> getStates(void);
};



}

