
#include <catch2/catch_test_macros.hpp>
#include "BasicGraphIr/Graph.hpp"
#include "NodeTest/NodeDictionary.hpp"

using namespace CorGIReg;

TEST_CASE("NodeDictionary Function Registration and Retrieval", "[NodeDictionary]") {
    // Define a function that checks if NodeNN's "value" info is greater than 10
    NodeDictionary::ConditionOnNode func1 = [](const std::shared_ptr<NodeNN>& node) {
        try {
            int value = node->getInfo<int>("value");
            return value > 10;
        } catch (...) {
            return false;
        }
    };

    // Register the function with the key "checkValue"
    bool registered = NodeDictionary::registerFunction("checkValue", func1);
    REQUIRE(registered == true);

    // Attempt to register another function with the same key
    NodeDictionary::ConditionOnNode funcDuplicate = [](const std::shared_ptr<NodeNN>& node) {
        try {
            int value = node->getInfo<int>("value");
            return value < 5;
        } catch (...) {
            return false;
        }
    };
    bool duplicateRegistered = NodeDictionary::registerFunction("checkValue", funcDuplicate);
    REQUIRE(duplicateRegistered == false);

    // Retrieve the function using the key "checkValue"
    NodeDictionary::ConditionOnNode retrievedFunc = NodeDictionary::at("checkValue");

    // Use the retrieved function
    auto node = std::make_shared<NodeNN>();
    node->setInfo("value", 15);
    bool result = retrievedFunc(node);
    REQUIRE(result == true);

    // Test with a node that should return false
    auto node2 = std::make_shared<NodeNN>();
    node2->setInfo("value", 5);
    bool result2 = retrievedFunc(node2);
    REQUIRE(result2 == false);

    // Test retrieval with an invalid key
    REQUIRE_THROWS_AS(NodeDictionary::at("invalidKey"), std::runtime_error);

    // Test registration with an invalid key (contains spaces)
    bool invalidKeyRegistered = NodeDictionary::registerFunction("invalid key", func1);
    REQUIRE(invalidKeyRegistered == false);

    // Test registration with invalid key (special characters)
    bool invalidKeyRegistered2 = NodeDictionary::registerFunction("invalid-key!", func1);
    REQUIRE(invalidKeyRegistered2 == false);
}

TEST_CASE("NodeDictionary Multiple Function Registrations", "[NodeDictionary]") {
    // Clear any previous registrations for testing (Not available in the current implementation)
    // For testing purposes, we can imagine this is a fresh start or extend the class to allow clearing

    // Define multiple functions
    NodeDictionary::ConditionOnNode funcEven = [](NodeNN::Ptr node) {
        try {
            int value = node->getInfo<int>("value");
            return value % 2 == 0;
        } catch (...) {
            return false;
        }
    };

    NodeDictionary::ConditionOnNode funcOdd = [](NodeNN::Ptr node) {
        try {
            int value = node->getInfo<int>("value");
            return value % 2 != 0;
        } catch (...) {
            return false;
        }
    };

    // Register functions with unique keys
    REQUIRE(NodeDictionary::registerFunction("isEven", funcEven) == true);
    REQUIRE(NodeDictionary::registerFunction("isOdd", funcOdd) == true);

    // Retrieve and test the functions
    auto nodeEven = std::make_shared<NodeNN>();
    nodeEven->setInfo("value", 4);

    auto nodeOdd = std::make_shared<NodeNN>();
    nodeOdd->setInfo("value", 5);

    REQUIRE(NodeDictionary::at("isEven")(nodeEven) == true);
    REQUIRE(NodeDictionary::at("isOdd")(nodeEven) == false);

    REQUIRE(NodeDictionary::at("isEven")(nodeOdd) == false);
    REQUIRE(NodeDictionary::at("isOdd")(nodeOdd) == true);
}

TEST_CASE("NodeDictionary Exception Handling", "[NodeDictionary]") {
    // Define a function that requires the "value" key
    NodeDictionary::ConditionOnNode funcRequiresValue = [](NodeNN::Ptr node) {
        int value = node->getInfo<int>("value"); // This may throw
        return value > 0;
    };

    // Register the function
    REQUIRE(NodeDictionary::registerFunction("requiresValue", funcRequiresValue) == true);

    // Create a node without the "value" key
    auto node = std::make_shared<NodeNN>();

    // Retrieve and use the function
    auto func = NodeDictionary::at("requiresValue");

    // Expect an exception when the function is called
    REQUIRE_THROWS_AS(func(node), std::runtime_error);
}
