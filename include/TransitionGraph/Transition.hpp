
#pragma once

#include "BasicGraphIr/Graph.hpp"
#include "TransitionGraph/State.hpp"
#include "NodeTest/NodeCondition.hpp"
#include "TransitionGraph/RunTimeContext.hpp"
#include "NodeTest/NodeDictionary.hpp"


#include <set>
#include <memory>
#include <map>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <limits>
#include <fmt/core.h>

#include <regex> //for factory
#include "TransitionGraph/State.hpp"
#include "TransitionGraph/RunTimeContext.hpp"

/** @brief the transition is the relation between two state in the TransitionGraph. 
 * the transition valid a run time context and give the next node(s) in the graph
 * there are multiple transition between two state depending of the  
 */
namespace CorGIReg{
    class State;
    class RunTimeContext;


    /** @brief the type of the transition 
     * TODO can we automatically register ??
    */
    enum class TransitionTypes {
        EMPTY = 0,
        REF,
        COMMON,
        STRICTCOMMON,
        UNIQUE,
        NNULL
    };

    struct TransitionResult {
        bool success;
        std::set<std::shared_ptr<NodeNN>> node;
    };


    class Transition: public std::enable_shared_from_this<Transition> {
    protected:
        /**
         * @brief the relative position to this transition relative to all the common Transition 
         * is use for expl in (C#>B)* wen you valid B you need to return to C
         * first is common id, second is the relative position negative if is a common parent
        */
        std::map<size_t,int> mRelativeToCommon;

        /**
         * @brief The ptr on the source state
        */
        std::shared_ptr<State> mStateSource;
        /**
         * @brief The ptr on the destination state
        */
        std::shared_ptr<State> mStateDest;

        /**
         * @brief the weak ptr of the transition to solve the recursion state have ptr on Transition and transition on state
        */
        std::weak_ptr<Transition> weakPtr;


        /** @brief the diction next nodes to child or to parent */
        bool mDirSourceToDest = true;

        /**
         * @brief Test to make on the node to validate
         * @see NodeCondition
        */
        //const std::shared_ptr<NodeCondition>  mToTest;

        const std::string mNodeTestKey;

        //const std::string mNodeTestKey;

    public:
            /**
         * @brief constructor of a transition between two state for a test
         * @param source the source state
         * @param dest the destination state
         * @param toTest the condition to validate to cross the transition
         * @details during construction, the transition is added to the source and destination state
         */
        Transition(std::shared_ptr<State>& source,std::shared_ptr<State>& dest, const std::string odeTestKey);
        virtual  ~Transition(){};
        Transition() : weakPtr(shared_from_this()) {}       

        /**
        *  @brief test is the validation of the node, it must be defined for all types of edge
        * it takes as argument an FSM traversal context and returns a set of next nodes
        *  @return set of next node or nullptr if not next
        */

        virtual const TransitionResult test(const std::shared_ptr<RunTimeContext>& context) =0;

        /**
         * @brief test if the transition is a common transition
         * @return true if is a common transition
         * @details a common transition is a transition that is relative to a common node
         * (i.e. a node that is a common ancestor of all the nodes in the graph)
         */
        virtual bool isCommon(void){ 
            return false; 
        }

         /**
         * @brief get the Common idx of the common test in this edge (if is a common edge)
         * @return idx of the common
        */
        virtual size_t getCommonIdx(void){
            return std::numeric_limits<std::size_t>::max();
        }
        /**
         * @brief get the relative postion to the common node deffine in this edge
         * @return map
        */
        inline const std::map<size_t,int>& getRelative(void){
            return mRelativeToCommon;
        }
        /**
         * @brief add new relative position
        */
        void updateRelative( const std::map<size_t,int>& relativePos );
        /**
         * @brief get source FsmNode
         * @return FsmNode
        */
        inline const std::shared_ptr<State>& getSourceState(void){
            return mStateSource;
        }

          /**
         * @brief get dest FsmNode
         * @return FsmNode
        */
        inline const std::shared_ptr<State>& getDestState(void){
            return mStateDest;
        }
        /**
         * @brief set a new dest to the edge
         * @return FsmNode
        */
        void reSetDestState(const std::shared_ptr<State> newDest);
        /**
         * @brief set a new source to the edge
         * @return FsmNode
        */
        void reSetSourceState(const std::shared_ptr<State> newSource);

        /**
         * @brief propagate the edge  mRelativePos to the others Edge and reset the relative position
        */
        void propagateRelativePos(void);
        /**
         * @brief update week ptr for the node, TODO best
        */
        void updateWeak(void);

        /**
         * @brief invert the next node direction node -> children or node -> parents 
        */
       void flipDir(void){
           mDirSourceToDest = !mDirSourceToDest;
       }
       /**
         * @brief test if the next node is a child
        */
       inline bool isDirectionIsChildren(void){
           return mDirSourceToDest;
       }

       const std::string getTestKey(void){
            return mNodeTestKey;
       }
       virtual std::string strRep(void) const;

    };


    /**
     * @brief class specialization for unique node validation transition st0 --A--> st1
     */
    class TransitionUnique:public Transition
    {
        public:
        TransitionUnique(std::shared_ptr<State>& source,std::shared_ptr<State>& dest, const std::string odeTestKey);
        const TransitionResult test(const std::shared_ptr<RunTimeContext>& context)  override;
        std::string strRep(void) const override;
    };

    /**
     * @enum CommonMode 
     * @brief enum that define the comportment of the rejection node of the common transition
     */
    enum class CommonMode {
        NONE = 0, /**< no node are rejected (the validation on the others child of parent of common node is not mandatory)*/
        FRONT,/** need to valid all the node in the same direction of the visit  */
        BACK,/** need to valid all the node in the opposite direction of the visit  */
        ALL /**< all child and parent of common  node must be valid by at least one FSM (no inclusion in a sub part of a graph is possible)*/
    };

     /**
     * @brief class specialization for  commun node transition st0 --A#--> st1
     * @see Transition
    */
    class TransitionCommon:public Transition
    {

        private:
        /**
         * @brief the map that defined the relation between the commonKey find by the lexer and a 
         * unique id use to refer to the common node, this map is common to all the common transition
        */
        static std::map<std::string,std::size_t> mCommonIdxMap;
        /**
         * @brief the common id test in this transition
        */
        std::size_t mCommonIdx;
        /**
         * @brief this defined the comportment of the rejection node of the common transition 
        */
        CommonMode mFlag;
        public:

        /**
         * @brief constructor  commun node ,
         * @details during construction,
         * the node key found by the lexer is converted to a unique id and the relative positions are updated.
        */
        TransitionCommon(std::shared_ptr<State>& source,std::shared_ptr<State>& dest, const std::string nodeTestKey,
        const std::string commonKey,CommonMode flag);

 
        const TransitionResult test(const std::shared_ptr<RunTimeContext>& context) override;
        bool isCommon(void) override {
            return true;
        }

        std::string strRep(void) const override;

    };



    /**
     * @brief class specialization for ref transition no test just go back in the operation graph
     * @see  Transition
    */
    class TransitionRef:public Transition
    {
        private:
        /**
         * @brief the pair of the common node id and the delta in term of child or parent 
         */
        const std::pair<std::size_t,int> mAnchor;

        public:
        /**
         * @brief constructor
         * @param source source state of the edge
         * @param dest dest state of the edge
         * @param refCommonIdx the id of one common node that we use as an anchor
         * @param deltaCommonIdx the delta in term of child or parent refer to the anchor
         */
        TransitionRef(std::shared_ptr<State>& source,std::shared_ptr<State>& dest);
        const TransitionResult test(const std::shared_ptr<RunTimeContext>& context) override;
        std::string strRep(void) const override;

        private:
        std::pair<std::size_t,int> getAnchor(std::shared_ptr<State>& dest) const;

    };

    /**
     * @brief class specialization for ref empty transition accept all test 
     * @see  Transition
    */
    class TransitionEmpty:public Transition
    {
        public:
        TransitionEmpty(std::shared_ptr<State>& source,std::shared_ptr<State>& dest);
        const TransitionResult test(const std::shared_ptr<RunTimeContext>& context) override;
        std::string strRep(void) const override;
    };


    /**
     * @brief class specialization for undefine transition reject all 
     * @see  Transition
    */
    class TransitionNone:public Transition
    {
        public:
        TransitionNone(std::shared_ptr<State>& source,std::shared_ptr<State>& dest);
        const TransitionResult test(const std::shared_ptr<RunTimeContext>& context) override;
        std::string strRep(void) const override;

    };

    /**
     * @brief class specialization for valid a nullptr , use to valid the end of Graph
     * @see  Transition
    */
    class TransitionNull:public Transition
    {
        public:
        TransitionNull(std::shared_ptr<State>& source,std::shared_ptr<State>& dest);
        const TransitionResult test(const std::shared_ptr<RunTimeContext>& context) override;
        std::string strRep(void) const override;
    };


/*
factory
 */




class TransitionFactory {
    public:
    /**
    * @brief factory for making edge and read the info in the lexeme of the token
    * @param source source node of the edge
    * @param dest Dest node of the edge
    * @param type type of the edge
    * @param lexeme the additional information to build the edge
    * @return s prt of the edge
    */
    static std::shared_ptr<Transition> make(std::shared_ptr<State> source,std::shared_ptr<State> dest,TransitionTypes type,const std::string lexeme = "");
   };




} // namespace CorGIReg