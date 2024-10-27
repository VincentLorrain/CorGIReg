#pragma once

#include <map>
#include <string>
#include <functional>
#include <regex>
#include "BasicGraphIr/Graph.hpp"

namespace CorGIReg {

class NodeDictionary {
public:
    // Define the function type: returns bool, takes NodeNN parameter
    typedef std::function<bool(const std::shared_ptr<NodeNN>&)> ConditionOnNode;

    // Static method to register a function with a given key
    static bool registerFunction(const std::string& key, ConditionOnNode func);

    // Static method to retrieve a registered function using the key
    static ConditionOnNode at(const std::string& key);

    static bool hasFunction(const std::string& key);

private:
    // Private constructor for singleton pattern
    NodeDictionary();

    // Delete copy constructor and assignment operator
    NodeDictionary(const NodeDictionary&) = delete;
    NodeDictionary& operator=(const NodeDictionary&) = delete;

    // Singleton instance accessor
    static NodeDictionary& getInstance();

    // Map to store functions with their unique keys
    std::map<std::string, ConditionOnNode> functionMap;

    // Instance method to register a function
    bool registerFunctionInstance(const std::string& key, ConditionOnNode func);

    // Instance method to retrieve a function
    ConditionOnNode atInstance(const std::string& key);

    // Instance method to check if a function exists
    bool hasFunctionInstance(const std::string& key) const;
};

} // namespace CorGIReg
