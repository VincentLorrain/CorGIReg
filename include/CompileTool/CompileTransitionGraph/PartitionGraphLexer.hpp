#pragma once

#include <string>
#include <memory>
#include <regex>
#include <stdexcept>
#include <sstream>

#include "CompileTool/Common/ParsingToken.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphTokens.hpp"

namespace CorGIReg {

    class PartitionGraphLexer
    {

    public:
    PartitionGraphLexer( const std::string gRegexExpressions );

    /// Read the next graph-pattern token. Invalid syntax raises a lexer error.
    std::shared_ptr<ParsingToken<tGTokensTypes>> getNextToken(void);
    /// Rewind to the beginning of the query.
    void rstPosition(void);

    /// Return whether the input has been consumed.
    bool isEnd(void);


    const std::string getQuery();


    /// Render the query with a marker at the current lexer position.
    const std::string rep();

    private:

    /// Build an error that points to an invalid token in the query.
    std::runtime_error badTokenError(const std::string& currentChars,std::size_t position);

    /// Complete graph pattern supplied by the caller.
    const std::string mRegularExpressions;
    /// Byte offset of the next character to read.
    std::size_t mPosition;

    };
}
