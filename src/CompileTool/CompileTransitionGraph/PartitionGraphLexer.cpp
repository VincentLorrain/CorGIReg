
#include "CompileTool/CompileTransitionGraph/PartitionGraphLexer.hpp"

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
    std::string currentChars = "";
    while (mPosition < mRegularExpressions.length())
    {
        //erase all space 
        if (mRegularExpressions[mPosition] != ' ')
        {
            currentChars += mRegularExpressions[mPosition];
        }
        else
        {
            mPosition++;
            continue;
        }

        /////
        // const lent token
        /////

        if (std::regex_match(currentChars,std::regex("\\->")))// the next TOKEN 
        {
            mPosition++;
            return std::make_shared<ParsingToken<tGTokensTypes>>(tGTokensTypes::NEXT,"");
        }
        else if (std::regex_match(currentChars,std::regex("\\<-"))) // the prec TOKEN
        {
            mPosition++;
            return std::make_shared<ParsingToken<tGTokensTypes>>(tGTokensTypes::PREC,"");
        }
        else if (std::regex_match(currentChars,std::regex("\\*")))// the * TOKEN 
        {
            mPosition++;
            return std::make_shared<ParsingToken<tGTokensTypes>>(tGTokensTypes::QZM,"");
        }
        else if (std::regex_match(currentChars,std::regex("\\+")))// the + TOKEN 
        {
            mPosition++;
            return std::make_shared<ParsingToken<tGTokensTypes>>(tGTokensTypes::QOM,"");
        }
        else if (std::regex_match(currentChars,std::regex("\\(")))// the LPAREN TOKEN 
        {
            mPosition++;
            return std::make_shared<ParsingToken<tGTokensTypes>>(tGTokensTypes::LPAREN,"");
        }
        else if (std::regex_match(currentChars,std::regex("\\)")))// the RPAREN TOKEN 
        {
            mPosition++;
            return std::make_shared<ParsingToken<tGTokensTypes>>(tGTokensTypes::RPAREN,"");
        }
        else if (std::regex_match(currentChars,std::regex("\\$")))// the END TOKEN 
        {
            mPosition++;
            return std::make_shared<ParsingToken<tGTokensTypes>>(tGTokensTypes::END,"");
        }
        else if (std::regex_match(currentChars,std::regex("\\?")))// the ZERO OR ONE TOKEN
        {
            mPosition++;
            return std::make_shared<ParsingToken<tGTokensTypes>>(tGTokensTypes::QZO,"");
        }

        //
        else if (std::regex_match(currentChars,std::regex(";")))// the SEP TOKEN 
        {
            //test if the last sep
            //std::string subStr = mRegularExpressions.substr(mPosition);
            mPosition++;
            return std::make_shared<ParsingToken<tGTokensTypes>>(tGTokensTypes::SEP,"");
        }

        /////
        //unconst lent token
        /////

        else if (std::regex_match(currentChars,std::regex("[A-Za-z_0-9]")))// the KEY or CKEY
        {   
            
            //the key is a common key
            bool isCKey = false;
            //the key is a strict common key
            bool isSCKey = false;

            std::regex keyRegex("[A-Za-z_0-9]+");
            std::regex cKeyRegex("[A-Za-z_0-9]+\\#[0-9]*");
            std::regex ccKeyRegex("[A-Za-z_0-9]+\\$[0-9]*");

            while ( mPosition < mRegularExpressions.length()) {

                // the currentchars is overs all the expressions bu one char , stop the recherche ans go back by one 
                if(!std::regex_match(currentChars,keyRegex) && !std::regex_match(currentChars,cKeyRegex) && !std::regex_match(currentChars,ccKeyRegex))
                {
                    currentChars.pop_back(); //the last char is the problems
                    break;
                }
                else if (std::regex_match(currentChars,cKeyRegex)){
                    isCKey = true;
                }
                else if (std::regex_match(currentChars,ccKeyRegex)){
                    isSCKey = true;
                }

                mPosition++;
                if (mPosition < mRegularExpressions.length()) currentChars += mRegularExpressions[mPosition];
                
            }
            //we end the match 2 possibility 
            //we are at the end of the mConditionalExpressions and we need to ensure the match
            //we are not we can continue
            if (mPosition == mRegularExpressions.length()-1)
            {
                if (!std::regex_match(currentChars,keyRegex) && !std::regex_match(currentChars,cKeyRegex) && !std::regex_match(currentChars,ccKeyRegex))
                {
                    throw badTokenError(currentChars,mPosition);
                }
            }


            if (isCKey && !isSCKey){
                return std::make_shared<ParsingToken<tGTokensTypes>>(tGTokensTypes::CKEY,currentChars);
            }else if(!isCKey && isSCKey){
                return std::make_shared<ParsingToken<tGTokensTypes>>(tGTokensTypes::SCKEY,currentChars);
            }else if(!isCKey && !isSCKey){
                return std::make_shared<ParsingToken<tGTokensTypes>>(tGTokensTypes::KEY,currentChars);
            }else{
                 throw badTokenError(currentChars,mPosition);
            }
        }

        mPosition++;
    }


    //no more to find no one match the currentChars 
    if (currentChars.empty()) {
        return  std::make_shared<ParsingToken<tGTokensTypes>>(tGTokensTypes::STOP,"");  // Null shared pointer ;
    }else{
        throw badTokenError(currentChars,mPosition);
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