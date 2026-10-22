#pragma once

#include "BasicGraphIr/Graph.hpp"
#include <map>
#include <string>
#include "NodeTest/NodeRule.hpp"

namespace CorGIReg
{
class NodeCondition
{
private:
    /** @brief the key is the symbol of the condition that valid the node in the regex like 
     * a part of the symbol dictionary
    */
    const std::string mKey;
    std::function<bool(std::shared_ptr<NodeNN>)> mTest;
public:
    NodeCondition(const std::string key,const std::string ConditionalExpressions);
    NodeCondition(const std::string key, const NodeValue& rule);
    NodeCondition(const std::string key, const char* expression)
        : NodeCondition(key, std::string(expression)) {}
    ~NodeCondition()=default;

    /** @brief test if the node is valid TODO */
    bool test(std::shared_ptr<NodeNN> node) const { return node && mTest(node); }

    /**
     * @brief get the condition key
     * @return the key
    */
    inline const std::string& getKey(){return mKey;}
};



}
