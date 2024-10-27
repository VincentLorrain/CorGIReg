#pragma once

#include <map>
#include <set>
#include <vector>
#include <memory>
#include <stdexcept>
#include <limits>
#include <algorithm>

#include "NodeTest/NodeCondition.hpp"
#include "BasicGraphIr/Graph.hpp"
#include "TransitionGraph/State.hpp"


/** @brief the run time context of the regex it link the operation graph and the transition graph
 * it contain all the information of a walk in the graph given a transition graph
 */

namespace CorGIReg{

class State;

class RunTimeContext
    {
    private:
        /**
         * @brief the list of node rejected for all the context
        */
        static std::vector<std::set<std::shared_ptr<NodeNN>>> mRejectedNodes;
        /**
         * @brief the actual state of this Context (where it's in the FSM graph)
        */
        std::shared_ptr<State> mActState;
        /**
         * @brief the actual node of this Context (where it's in the graph)
        */
        std::shared_ptr<NodeNN> mActOpNode;
        /**
         * @brief the map of the node consider as common and the common ID
         * @details we need to store what node it's consider as common because of the end
         * resolution of the matching, all node consider as common need to be the same in all context
        */
        std::map<std::shared_ptr<NodeNN>,std::size_t> mCommonNodes;
        /**
         * @brief the map of the node that as been valid in this context , and the test that valid the node
         * is use to link the node to the part of the query that match this node 
        */
        std::map<std::string,std::set<std::shared_ptr<NodeNN>>> mValidNodes;
        /**
         * @brief the index in the rejected node of this context, is uid of the context 
        */
        std::size_t mLocalIdxRejected;

    public:
        /**
         * @brief constructor of a new RunTimeContext
         */
        RunTimeContext(std::shared_ptr<State> actState ,std::shared_ptr<NodeNN> actOpNode ,std::size_t idxRejected =std::numeric_limits<std::size_t>::max() );
        /**
         * @brief constructor of a new RunTimeContext from a RunTimeContext, copy of the sate and the position in the graph and in the transition graph
         */
        RunTimeContext(std::shared_ptr<RunTimeContext> fsmRunTime);
          /**
         * @brief constructor of a new RunTimeContext from a RunTimeContext, copy of the sate new position in the graph and in the transition graph
         */
        RunTimeContext(std::shared_ptr<RunTimeContext> fsmRunTime,std::shared_ptr<State> actState ,std::shared_ptr<NodeNN> actOpNode );
        ~RunTimeContext() =default;






        inline void addRejectedNode(std::shared_ptr<NodeNN>  node){
            mRejectedNodes[mLocalIdxRejected].insert(node);
        }
        /**
         * @brief get the rejected nodes of this context
         */
        inline std::set<std::shared_ptr<NodeNN>> getRejectedNodes(void) const {
            return mRejectedNodes[mLocalIdxRejected];
        }


        inline std::shared_ptr<NodeNN> getActNode(void){
            return mActOpNode;
        }

        inline const std::map<std::string,std::set<std::shared_ptr<NodeNN>>>& getValid(void){
            return mValidNodes;
        }

        /**
         * @brief return true if the actual state is a valid state in the transition graph that is a match
         * @return true if the actual state is a valid state in the transition graph
         */
 
        void setCommon(std::shared_ptr<NodeNN> node,std::size_t commonIdx);
        std::shared_ptr<NodeNN> getCommonNodeFromIdx(const std::size_t& commonIdx);
        std::set<std::shared_ptr<NodeNN>> getCommonNodes(void) const;
        bool isCommonDefined(const std::shared_ptr<NodeNN>& node);

        /**
         * get the origins of the state
         */
        std::set<std::size_t> getSubStmId(void) const;
        
        bool areEqual(const std::shared_ptr<RunTimeContext>& context) const;

        bool areCompatible(const std::shared_ptr<RunTimeContext>& context) const;


        /*
        Valid
        nodes 
        */

        /**
         * 
         */
        bool isOnValidState(void);
        std::set<std::shared_ptr<NodeNN>> getValidNodes(void) const;
        bool isAlreadyValid(std::shared_ptr<NodeNN>  node);
        void setValid(std::shared_ptr<NodeNN>  node,const std::string  tag);
        std::set<std::shared_ptr<NodeNN>> getValidNodesNoCommon(void) const;

        std::shared_ptr<State> getActState();
};
    


}