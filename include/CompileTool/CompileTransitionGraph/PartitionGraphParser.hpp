#pragma once

#include <memory>
#include <string>
#include <vector>

#include "CompileTool/Common/AstNode.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphLexer.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphTokens.hpp"


namespace CorGIReg{

/// Recursive-descent parser for graph patterns. The AST is shared by matching
/// and graph generation.
class PartitionGraphParser {

public:
    /// Store the complete query and read its first token.
    PartitionGraphParser(const std::string gRegexExpressions);

    ~PartitionGraphParser() noexcept;

    /// Parse and validate one query. Top-level convergence becomes two paths
    /// that share an anchor. After reading the AST, rewind the lexer for reuse.
    std::shared_ptr<AstNode<tGTokensTypes>> parse(void);


    /// Return the original query text, including any convergence shorthand.
    const std::string getQuery();


private:
    /// Rewind the lexer and restore the first lookahead token.
    void rstParser(void);

    /// Require the lookahead token and advance to the next token.
    void ackToken(tGTokensTypes  tokenType);

    /// Read one key, anchor, or group, including any permitted postfix quantifier.
    std::shared_ptr<AstNode<tGTokensTypes>> constructAstExp(void);

    /// Read a sequence. A top-level call can return a converging path through
    /// incoming when a forward sequence meets reverse arrows at an anchor.
    std::shared_ptr<AstNode<tGTokensTypes>> constructAstSeq(
        std::shared_ptr<AstNode<tGTokensTypes>>* incoming = nullptr, bool afterForward = false);

    /// Read the right operand of a reverse arrow, including further reverse arrows.
    std::shared_ptr<AstNode<tGTokensTypes>> constructAstRseq(void);

    /// Read a parenthesized group. A group with alternatives may omit its quantifier.
    std::shared_ptr<AstNode<tGTokensTypes>> constructAstDomain(void);

    /// Combine top-level paths, including paths expanded from convergence.
    std::shared_ptr<AstNode<tGTokensTypes>> constructAstAllExpr(void);


    /// Lookahead token consumed by the next grammar rule.
    std::shared_ptr<ParsingToken<tGTokensTypes>> mCurrentToken;

    PartitionGraphLexer mLexer;

};


}

