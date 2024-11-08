//PGInterpreterTG

#pragma once



#include <string>
#include <vector>
#include <map>
#include <memory>
#include <random>
#include <ctime>
#include <regex>

#include "BasicGraphIr/Graph.hpp"
#include "CompileTool/Common/AstNode.hpp"
#include "NodeTest/NodeCondition.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphLexer.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphParser.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphTokens.hpp"

namespace CorGIReg {

    class PGInterpreterIR
    {
    private:

        std::map<std::string,std::set<std::shared_ptr<NodeNN>>> mCommonNodes;
        size_t mMaxLoops = 10;

        PartitionGraphParser mParser;
        std::size_t mActGroupe;
        const std::string mGraphMatchExpr;
        
    public:
        PGInterpreterIR(const std::string graphMatchExpr);
        virtual ~PGInterpreterIR() =default;

        std::shared_ptr<GraphNN>  interpret(void);



        private:
        /**
         * utility function
         */

        

        std::shared_ptr<GraphNN> unionIR(std::shared_ptr<GraphNN>& leftTG,std::shared_ptr<GraphNN>& rightTG);

        void deleteIR(std::shared_ptr<GraphNN>& tg);

        
        /**
         * 
         */
        std::shared_ptr<GraphNN> visit(std::shared_ptr<AstNode<tGTokensTypes>> AstTree);

        /**
         * @ingroup graphTGInterpreterF
         * @brief leaf of tg make the tg for test one transition
         */
        std::shared_ptr<GraphNN> keyF(std::shared_ptr<AstNode<tGTokensTypes>> AstNode);

        /**
         * @ingroup graphTGInterpreterF
         * @brief  leaf of tg make the tg for test end of the graph
         */
        std::shared_ptr<GraphNN> endF(std::shared_ptr<AstNode<tGTokensTypes>> AstNode);

        /**
         * @ingroup graphTGInterpreterF
         * @brief combine two tg of two expression.
         */
        std::shared_ptr<GraphNN> sepF(std::shared_ptr<GraphNN> leftTG,std::shared_ptr<GraphNN> rightTG);
        /**
         * @ingroup graphTGInterpreterF
         * @brief combine two to make a new that match leftTG next rightTG
         */
        std::shared_ptr<GraphNN> nextF(std::shared_ptr<GraphNN> leftTG,std::shared_ptr<GraphNN> rightTG);

       /**
         * @ingroup graphTGInterpreterF
         * @brief combine two to make a new that match leftTG prec rightTG
         */
        std::shared_ptr<GraphNN> precF(std::shared_ptr<GraphNN> leftTG,std::shared_ptr<GraphNN> rightTG);
            
        /**
         * @ingroup graphTGInterpreterF
         * @brief make the tg match +
         */
        std::shared_ptr<GraphNN> qomF(std::shared_ptr<GraphNN> tg);
        /**
         * @ingroup graphTGInterpreterF
         * @brief  make the tg match *
         */
        std::shared_ptr<GraphNN> qzmF(std::shared_ptr<GraphNN> tg);
        /**
         * @ingroup graphTGInterpreterF
         * @brief  make the tg match ?
         */
        std::shared_ptr<GraphNN> qzoF(std::shared_ptr<GraphNN> tg);

    };



}



