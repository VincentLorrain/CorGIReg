
#pragma once

#include <string>
#include <type_traits>
#include <vector>
#include <memory>
#include "CompileTool/Common/ParsingToken.hpp"

namespace CorGIReg{

    template <typename EnumType>
    class AstNode: public std::enable_shared_from_this<AstNode<EnumType>>
    {
        static_assert(std::is_enum<EnumType>::value, "AstNode EnumType must be an enum type");
        public:

        AstNode(std::shared_ptr<ParsingToken<EnumType>> token,std::vector<std::shared_ptr<AstNode<EnumType>>> child ={}):mToken(token),mChild(child){}
        /**
         * @brief get the type of the token
         * @return the type
         */
        EnumType getType() const{
            return mToken->getType();
        }
        /**
         * @brief get the lexeme of the token
         * @return the lexeme
         */
        const std::string  getValue() const{
            return mToken->getLexeme();
        }
        /**
         * @brief get the child of the node
         * @return child
         */
        const std::vector<std::shared_ptr<AstNode>>& getChilds() const {
            return mChild;
        }
        /**
         * @brief test if the node is a leaf in the tree
         * @return true if a leaf
         */
        bool isLeaf() const {
            return mChild.size() == 0;
        }

        /**
         * @brief get the number of child
         * @return the number of child
         */
        std::size_t nbChild() const{
            return mChild.size();
        }
        private:
        /**
         * @brief the token of the node
         */
        const std::shared_ptr<ParsingToken<EnumType>> mToken;
        /**
         * @brief list of child
         */
        const std::vector<std::shared_ptr<AstNode>> mChild;
    };}


