//PGInterpreterTG

#pragma once



#include <string>
#include <vector>
#include <map>
#include <memory>


#include "TransitionGraph/TransitionGraph.hpp"
#include "CompileTool/Common/AstNode.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphLexer.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphParser.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphTokens.hpp"
#include "CompileTool/PostAstOptimization/RootSelectionPass.hpp"

namespace CorGIReg {

    class PGInterpreterTG
    {
    private:
        /* data */
        PartitionGraphParser mParser;
        std::size_t mActGroupe;
        const std::string mGraphMatchExpr;
        RootSelectionPlan mRootSelectionPlan;
        std::map<const AstNode<tGTokensTypes>*, std::size_t> mQuantifierIndices;
        void indexQuantifiers(const std::shared_ptr<AstNode<tGTokensTypes>>& node);
        std::shared_ptr<TransitionGraph> quantify(std::shared_ptr<TransitionGraph> body,
                                               std::size_t index, char symbol);
        
    public:
        PGInterpreterTG(const std::string graphMatchExpr);
        virtual ~PGInterpreterTG() =default;

        std::shared_ptr<TGCompiled>  interpret(void);
        const RootSelectionPlan& getRootSelectionPlan() const { return mRootSelectionPlan; }

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
            

    };



}



