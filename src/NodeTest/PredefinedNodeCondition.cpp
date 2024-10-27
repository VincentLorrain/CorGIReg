#include "NodeTest/PredefinedNodeCondition.hpp"

namespace CorGIReg {

void PredefinedFunctions::registerFunctions() {
    // Function to check if the "value" info is greater than 10
    NodeDictionary::ConditionOnNode isConv = [](const std::shared_ptr<NodeNN>& node) {
        return node->getType() == "Conv";
    };
    NodeDictionary::registerFunction("Conv", isConv);

    // Function to check if the node has a specific type
    NodeDictionary::ConditionOnNode isFc = [](const std::shared_ptr<NodeNN>& node) {
        return node->getType() == "Fc";
    };
    NodeDictionary::registerFunction("Fc", isFc);

    //TODO add more predefined functions
}

} // namespace CorGIReg
