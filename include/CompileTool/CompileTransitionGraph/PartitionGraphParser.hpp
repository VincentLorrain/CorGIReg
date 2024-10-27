#pragma once

#include <memory>
#include <string>
#include <vector>

#include "CompileTool/Common/AstNode.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphLexer.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphTokens.hpp"


namespace CorGIReg{

/**
 * @brief this class uses the lexer to create an AST according to a set of gramer rules
 */
class PartitionGraphParser {

public:
    /**
     * @brief AST graph creation function
     * @param gRegexExpressions String representing the logical fuction to be performed
     */
    PartitionGraphParser(const std::string gRegexExpressions);

    ~PartitionGraphParser() noexcept;

    /**
     * @brief AST graph creation function
     * @return The AST tree
     */
    std::shared_ptr<AstNode<tGTokensTypes>> parse(void);


    /**
     * @brief get the query that be use in the parsing
     * @return query
     */
    const std::string getQuery();


private:
    /**
     * @brief restart at the start of the ConditionalExpressions for LEXER and restart  mCurrentToken
     */
    void rstParser(void);

    //////////////////

    /**
     * @defgroup ParsingFunctions Function for creating AST
     * @brief Functions for recursive construction of the AST representing grammar rules
     */

    /**
     * @ingroup ParsingFunctions
     * @brief Token reading and verification function
     *
     */
    void ackToken(tGTokensTypes  tokenType);

    //TODO TODO
    /**
     * @ingroup ParsingFunctions
     * @brief Function of grammar rules for key :  KEY(QOM | QZM)? | CKEY
     * @return AST node
     */
    std::shared_ptr<AstNode<tGTokensTypes>> constructAstExp(void);

    /**
    * @ingroup ParsingFunctions
    * @brief Function of grammar rules for sequence :  seq : exp ((NEXT seq)* | NEXT END)
    * @return AST node
    */
    std::shared_ptr<AstNode<tGTokensTypes>> constructAstSeq(void);

    /**
    * @ingroup ParsingFunctions
    * @brief Function of grammar rules for sequence :  rseq : exp ((PREC rseq)* )
    * @return AST node
    */
    std::shared_ptr<AstNode<tGTokensTypes>> constructAstRseq(void);

    /**
    * @ingroup ParsingFunctions
    * @brief Function of grammar rules for domain : (seq NEXT domain)? | LPAREN domain RPAREN (QOM | QZM) (NEXT domain)?
    * @return AST node
    */
    std::shared_ptr<AstNode<tGTokensTypes>> constructAstDomain(void);

    /**
    * @ingroup ParsingFunctions
    * @brief Function of grammar rules for multiple exepresion : allExpr: domain (SEP allExpr)*
    * @return AST node
    */
    std::shared_ptr<AstNode<tGTokensTypes>> constructAstAllExpr(void);


    /**
    * @brief The actual token in the parce
    */
    std::shared_ptr<ParsingToken<tGTokensTypes>> mCurrentToken;

    /**
    * @brief The lexem use
    */
    PartitionGraphLexer mLexer;

};


}

