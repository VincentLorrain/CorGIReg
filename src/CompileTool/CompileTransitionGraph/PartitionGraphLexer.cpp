
#include "CompileTool/CompileTransitionGraph/PartitionGraphLexer.hpp"
#include <cctype>
#include <limits>
#include "GraphAccess/Ports.hpp"

using namespace CorGIReg; 


PartitionGraphLexer::PartitionGraphLexer( const std::string gRegexExpressions ):
mRegularExpressions(gRegexExpressions){
    mPosition = 0;
}

/**
 * @brief get the next token on the gRegexExpressions
 * @return std::shared_ptr of ParsingToken
 * @throw std::runtime_error if the token is not recognized
 */

std::shared_ptr<ParsingToken<tGTokensTypes>> PartitionGraphLexer::getNextToken(void){
    auto keyChar = [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9') || c == '_';
    };
    while (mPosition < mRegularExpressions.size() &&
           std::isspace(static_cast<unsigned char>(mRegularExpressions[mPosition]))) ++mPosition;
    if (mPosition == mRegularExpressions.size())
        return std::make_shared<ParsingToken<tGTokensTypes>>(tGTokensTypes::STOP, "");
    const auto start = mPosition;
    const char current = mRegularExpressions[mPosition];
    const bool prefix = current == '$' && mPosition + 1 < mRegularExpressions.size() &&
                        keyChar(mRegularExpressions[mPosition + 1]);
    if (prefix || keyChar(current)) {
        if (prefix) ++mPosition;
        while (mPosition < mRegularExpressions.size() && keyChar(mRegularExpressions[mPosition])) ++mPosition;
        char suffix = 0;
        if (mPosition < mRegularExpressions.size() &&
            (mRegularExpressions[mPosition] == '#' || mRegularExpressions[mPosition] == '$')) {
            suffix = mRegularExpressions[mPosition++];
            while (mPosition < mRegularExpressions.size() &&
                   mRegularExpressions[mPosition] >= '0' && mRegularExpressions[mPosition] <= '9') ++mPosition;
        }
        auto type = prefix || suffix == '$' ? tGTokensTypes::SCKEY :
                    suffix == '#' ? tGTokensTypes::CKEY : tGTokensTypes::KEY;
        return std::make_shared<ParsingToken<tGTokensTypes>>(type, mRegularExpressions.substr(start, mPosition - start));
    }
    auto emit = [&](tGTokensTypes type, std::size_t size = 1) {
        mPosition += size;
        return std::make_shared<ParsingToken<tGTokensTypes>>(type, "");
    };
    if (mRegularExpressions.compare(mPosition, 2, "->") == 0 ||
        mRegularExpressions.compare(mPosition, 2, "<-") == 0) {
        const auto type = current == '-' ? tGTokensTypes::NEXT : tGTokensTypes::PREC;
        mPosition += 2;
        const auto begin = mPosition;
        if (mPosition < mRegularExpressions.size() && mRegularExpressions[mPosition] == '*') ++mPosition;
        else while (mPosition < mRegularExpressions.size() && std::isdigit(static_cast<unsigned char>(mRegularExpressions[mPosition]))) ++mPosition;
        auto value = mRegularExpressions.substr(begin, mPosition - begin);
        if (!value.empty() && value != "*") {
            try {
                auto number = std::stoull(value);
                if (!number || number > std::numeric_limits<PortNumber>::max()) throw std::out_of_range("port");
            } catch (const std::exception&) { throw badTokenError("Invalid port", begin); }
        }
        return std::make_shared<ParsingToken<tGTokensTypes>>(type, value);
    }
    switch (current) {
    case '*': return emit(tGTokensTypes::QZM);
    case '+': return emit(tGTokensTypes::QOM);
    case '?': return emit(tGTokensTypes::QZO);
    case '(': return emit(tGTokensTypes::LPAREN);
    case ')': return emit(tGTokensTypes::RPAREN);
    case ';': return emit(tGTokensTypes::SEP);
    case '$': return emit(tGTokensTypes::END);
    default: throw badTokenError(std::string(1, current), mPosition);
    }
}

void PartitionGraphLexer::rstPosition(void){
    if (isEnd()){
        mPosition = 0;
    }else{
        throw badTokenError("end rst",mPosition);
    }
}

bool PartitionGraphLexer::isEnd(void){
    return mPosition >= mRegularExpressions.length();
}


const std::string PartitionGraphLexer::getQuery(){
    return mRegularExpressions;
}

std::runtime_error PartitionGraphLexer::badTokenError(const std::string& currentChars,std::size_t position){
    std::ostringstream errorMessage;
    errorMessage << "\nBad syntax " << currentChars << " :\n" << mRegularExpressions << "\n";
    for (std::size_t i = 0; i < position; i++) {
        errorMessage << ' ';
    }
    errorMessage << "^\n";

    return std::runtime_error(errorMessage.str());
}

  const std::string PartitionGraphLexer::rep(){
    std::string out = mRegularExpressions;
    out += "\n";
    for (std::size_t i = 0; i < mPosition; i++) {
        out += ' ';
    }
    out +=  "^\n";
    return out ;
    }
