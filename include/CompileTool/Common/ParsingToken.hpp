#pragma once

#include <string>
#include <type_traits>
#include <sstream> // Include the necessary header

namespace CorGIReg{

    template <typename EnumType>
    class ParsingToken: public std::enable_shared_from_this<ParsingToken<EnumType>>
    {
        static_assert(std::is_enum<EnumType>::value, "ParsingToken EnumType must be an enum type");
        public:
        /**
         * @brief Token container
         * @param type one of the token type
         * @param lexeme String representing aditional information of the token
         */
        ParsingToken(const EnumType type , const std::string lexeme ):mLexeme(lexeme),mType(type){}

        /**
         * @brief get the lexeme
         * @return std::string
         */
        const std::string getLexeme(void){
            return mLexeme;
        }

        /**
         * @brief get the token type
         *
         * @return ParsingToken
         */
        const EnumType getType(void){
            return mType;
        }

        /**
         * @brief copy the token
         * @return deep copy of the token
         */
        std::shared_ptr<ParsingToken> copy(){
            auto newToken = std::make_shared<ParsingToken<EnumType>>(mType,mLexeme);
            return newToken;
        }

        //TODO
        std::ostringstream rep(void){
            std::ostringstream out;
            out << " Token ("  << mLexeme <<")" << "\n";
            return out;
        }

        private:

        /**
         * @brief additional information of the token
         */
        const std::string mLexeme;

        /**
         * @brief type of the token
         * @see ConditionalTokenTypes
         */
        const EnumType mType;

    };
}


