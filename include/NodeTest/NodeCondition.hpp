#pragma once

#include "BasicGraphIr/Graph.hpp"
#include <map>
#include <string>

namespace CorGIReg
{
class NodeCondition
{
private:
    /** @brief the key is the symbol of the condition that valid the node in the regex like 
     * a part of the symbol dictionary
    */
    const std::string mKey;
public:
    NodeCondition(const std::string key,const std::string ConditionalExpressions);
    ~NodeCondition()=default;

    /** @brief test if the node is valid TODO */
    bool test(std::shared_ptr<NodeNN> node){return true;};

    /**
     * @brief get the condition key
     * @return the key
    */
    inline const std::string& getKey(){return mKey;}
};



}
