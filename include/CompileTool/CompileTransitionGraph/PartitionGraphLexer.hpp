#pragma once

#include <string>
#include <memory>
#include <regex>
#include <stdexcept> //error
#include <sstream>

#include "CompileTool/Common/ParsingToken.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphTokens.hpp"

namespace CorGIReg {

    class PartitionGraphLexer
    {

    public:
    PartitionGraphLexer( const std::string gRegexExpressions );

    /**
     * @brief Get the next token on the gRegexExpressions
     * @return ConditionalToken
     */
    std::shared_ptr<ParsingToken<tGTokensTypes>> getNextToken(void);
    /**
     * @brief Restart at the start of the gRegexExpressions
     *
     */
    void rstPosition(void);

    /**
     * @brief Test if the string is completely read
     * @return bool
     */
    bool isEnd(void);


    const std::string getQuery();


    /**
     * @brief Get the representation of the class
     * @return string
     */
    const std::string rep();

    private:

    /**
     * @brief Constructs an error message to display the character not understood by the lexer
     * @return error message
     */
    std::runtime_error badTokenError(const std::string& currentChars,std::size_t position);

    /**
     * @brief The expression of the test to be performed on the nodes
     */
    const std::string mRegularExpressions;
    /**
     * @brief The lexer's current position in mConditionalExpressions
     */
    std::size_t mPosition;

    };
}
