#include "NodeTest/NodeDictionary.hpp"
#include <stdexcept>

namespace CorGIReg {

NodeDictionary::NodeDictionary() {}

NodeDictionary& NodeDictionary::getInstance() {
    static NodeDictionary instance;
    return instance;
}

bool NodeDictionary::registerFunction(const std::string& key, ConditionOnNode func) {
    return getInstance().registerFunctionInstance(key, func);
}

NodeDictionary::ConditionOnNode NodeDictionary::at(const std::string& key) {
    return getInstance().atInstance(key);
}

bool NodeDictionary::hasFunction(const std::string& key) {
    return getInstance().hasFunctionInstance(key);
}

bool NodeDictionary::registerFunctionInstance(const std::string& key, ConditionOnNode func) {
    // Ensure key matches the regex pattern [a-zA-Z0-9_]+
    if (!std::regex_match(key, std::regex("^[a-zA-Z0-9_]+$"))) {
        return false; // Key does not match the pattern
    }

    // Check if the key already exists
    if (functionMap.find(key) != functionMap.end()) {
        return false; // Registration fails if key is already used
    }

    // Register the function
    functionMap[key] = func;
    return true;
}

NodeDictionary::ConditionOnNode NodeDictionary::atInstance(const std::string& key) {
    auto it = functionMap.find(key);
    if (it != functionMap.end()) {
        return it->second; // Return the function if found
    } else {
        throw std::runtime_error("Function with key '" + key + "' not found.");
    }
}

bool NodeDictionary::hasFunctionInstance(const std::string& key) const {
    return functionMap.find(key) != functionMap.end();
}


} // namespace CorGIReg
