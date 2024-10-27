//PGInterpreterTG

#pragma once



#include <string>
#include <vector>
#include <map>
#include <memory>


#include "TransitionGraph/TransitionGraph.hpp"
#include "CompileTool/Common/AstNode.hpp"
#include "NodeTest/NodeCondition.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphLexer.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphParser.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphTokens.hpp"

namespace CorGIReg {

    class PGInterpreterTG
    {
    private:
        /* data */
        PartitionGraphParser mParser;
        std::size_t mActGroupe;
        const std::string mGraphMatchExpr;
        
    public:
        PGInterpreterTG(const std::string graphMatchExpr);
        virtual ~PGInterpreterTG() =default;

        std::shared_ptr<TGCompiled>  interpret(void);

        private:

        std::shared_ptr<TransitionGraph> visit(std::shared_ptr<AstNode<tGTokensTypes>> AstTree);

        /**
         * @ingroup graphFsmInterpreterF
         * @brief leaf of fsm make the fsm for test one transition
         */
        std::shared_ptr<TransitionGraph> keyF(std::shared_ptr<AstNode<tGTokensTypes>> AstNode);

        /**
         * @ingroup graphFsmInterpreterF
         * @brief  leaf of fsm make the fsm for test end of the graph
         */
        std::shared_ptr<TransitionGraph> endF(std::shared_ptr<AstNode<tGTokensTypes>> AstNode);

        /**
         * @ingroup graphFsmInterpreterF
         * @brief combine two fsm of two expression.
         */
        std::shared_ptr<TransitionGraph> sepF(std::shared_ptr<TransitionGraph> leftFsm,std::shared_ptr<TransitionGraph> rightFsm);
        /**
         * @ingroup graphFsmInterpreterF
         * @brief combine two to make a new that match leftFsm next rightFsm
         */
        std::shared_ptr<TransitionGraph> nextF(std::shared_ptr<TransitionGraph> leftFsm,std::shared_ptr<TransitionGraph> rightFsm);

       /**
         * @ingroup graphFsmInterpreterF
         * @brief combine two to make a new that match leftFsm prec rightFsm
         */
        std::shared_ptr<TransitionGraph> precF(std::shared_ptr<TransitionGraph> leftFsm,std::shared_ptr<TransitionGraph> rightFsm);
            
        /**
         * @ingroup graphFsmInterpreterF
         * @brief make the fsm match +
         */
        std::shared_ptr<TransitionGraph> qomF(std::shared_ptr<TransitionGraph> fsm);
        /**
         * @ingroup graphFsmInterpreterF
         * @brief  make the fsm match *
         */
        std::shared_ptr<TransitionGraph> qzmF(std::shared_ptr<TransitionGraph> fsm);
        /**
         * @ingroup graphFsmInterpreterF
         * @brief  make the fsm match ?
         */
        std::shared_ptr<TransitionGraph> qzoF(std::shared_ptr<TransitionGraph> fsm);

    };



}



