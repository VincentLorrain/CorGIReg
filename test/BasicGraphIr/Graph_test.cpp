#include <catch2/catch_test_macros.hpp>
#include "BasicGraphIr/Graph.hpp"


using namespace CorGIReg;

TEST_CASE("NodeNN and GraphNN: Extended Tests") {

    SECTION("Setting and Getting Info with Various Data Types") {
        auto node = std::make_shared<NodeNN>();

        // Test setting and getting string
        node->setInfo("stringKey", std::string("stringValue"));
        REQUIRE(node->getInfo<std::string>("stringKey") == "stringValue");

        // Test setting and getting integer
        node->setInfo("intKey", 42);
        REQUIRE(node->getInfo<int>("intKey") == 42);

        // Test setting and getting vector
        std::vector<int> vec = {1, 2, 3};
        node->setInfo("vectorKey", vec);
        REQUIRE(node->getInfo<std::vector<int>>("vectorKey") == vec);

        // Test setting and getting set
        std::set<std::string> strSet = {"apple", "banana", "cherry"};
        node->setInfo("setKey", strSet);
        REQUIRE(node->getInfo<std::set<std::string>>("setKey") == strSet);

        // Test hasArgument method
        REQUIRE(node->hasArgument("stringKey") == true);
        REQUIRE(node->hasArgument("nonexistentKey") == false);

        // Test getType method
        node->setInfo("type", std::string("CustomType"));
        REQUIRE(node->getType() == "CustomType");
    }

    SECTION("Exception Handling in getInfo") {
        auto node = std::make_shared<NodeNN>();

        node->setInfo("intKey", 42);

        // Test exception when key does not exist
        REQUIRE_THROWS_AS(node->getInfo<int>("nonexistentKey"), std::runtime_error);

        // Test exception when type mismatch occurs
        REQUIRE_THROWS_AS(node->getInfo<std::string>("intKey"), std::runtime_error);

        // Test getInfo with default value
        REQUIRE(node->getInfo<int>("nonexistentKey", -1) == -1);
        REQUIRE(node->getInfo<std::string>("nonexistentKey", "default") == "default");
    }

    SECTION("Adding Parents and Children") {
        auto graph = std::make_shared<GraphNN>();
        auto from = std::make_shared<NodeNN>();
        auto to = std::make_shared<NodeNN>();

        graph->addEdge(from, to);

        REQUIRE(to->getParents().size() == 1);
        REQUIRE(from->getChildren().size() == 1);
        REQUIRE(to->getParents() == std::set<NodeNN::Ptr>{from});
        REQUIRE(from->getChildren() == std::set<NodeNN::Ptr>{to});
    }

    SECTION("getNodeDelta Functionality") {
        auto graph = std::make_shared<GraphNN>();
        auto nodeA = std::make_shared<NodeNN>();
        auto nodeB = std::make_shared<NodeNN>();
        auto nodeC = std::make_shared<NodeNN>();

        graph->addEdge(nodeA, nodeB);
        graph->addEdge(nodeB, nodeC);

        std::set<NodeNN::Ptr> nodeSee;

        // Delta = 0
        auto delta0 = nodeA->getNodeDelta(0, nodeSee);
        REQUIRE(delta0 == std::set<NodeNN::Ptr>{nodeA});

        // Delta = 1
        nodeSee.clear();
        auto delta1 = nodeA->getNodeDelta(1, nodeSee);
        REQUIRE(delta1 == std::set<NodeNN::Ptr>{nodeB});

        // Delta = 2
        nodeSee.clear();
        auto delta2 = nodeA->getNodeDelta(2, nodeSee);
        REQUIRE(delta2 == std::set<NodeNN::Ptr>{nodeC});

        // Negative Delta
        nodeSee.clear();
        auto deltaNeg1 = nodeC->getNodeDelta(-1, nodeSee);
        REQUIRE(deltaNeg1 == std::set<NodeNN::Ptr>{nodeB});

        nodeSee.clear();
        auto deltaNeg2 = nodeC->getNodeDelta(-2, nodeSee);
        REQUIRE(deltaNeg2 == std::set<NodeNN::Ptr>{nodeA});
    }

    SECTION("Cycle Detection and Loop Avoidance in getNodeDelta") {
        auto graph = std::make_shared<GraphNN>();
        auto node1 = std::make_shared<NodeNN>();
        auto node2 = std::make_shared<NodeNN>();
        auto node3 = std::make_shared<NodeNN>();

        // Creating a cycle: node1 -> node2 -> node3 -> node1
        graph->addEdge(node1, node2);
        graph->addEdge(node2, node3);
        graph->addEdge(node3, node1);

        std::set<NodeNN::Ptr> nodeSee;

        // Delta = 3 (should not enter infinite loop)
        nodeSee.clear();
        auto delta3 = node1->getNodeDelta(3, nodeSee);
        REQUIRE(delta3.empty());

        // Delta = -1
        nodeSee.clear();
        auto deltaNeg1 = node2->getNodeDelta(-1, nodeSee);
        REQUIRE(deltaNeg1 == std::set<NodeNN::Ptr>{node1});

        // Delta = 2
        nodeSee.clear();
        auto delta2 = node1->getNodeDelta(2, nodeSee);
        REQUIRE(delta2 == std::set<NodeNN::Ptr>{node3});
    }

    SECTION("GraphNN Node Management") {
        auto graph = std::make_shared<GraphNN>();
        auto node1 = std::make_shared<NodeNN>();
        auto node2 = std::make_shared<NodeNN>();

        graph->addNode(node1);
        graph->addNode(node2);
        graph->addNode(node1); // Adding duplicate node

        REQUIRE(graph->getNodes().size() == 2);

        // Verify that both nodes are in the graph
        auto nodes = graph->getNodes();
        REQUIRE(std::find(nodes.begin(), nodes.end(), node1) != nodes.end());
        REQUIRE(std::find(nodes.begin(), nodes.end(), node2) != nodes.end());
    }

    SECTION("hasArgument Method") {
        auto node = std::make_shared<NodeNN>();
        node->setInfo("exists", 1);

        REQUIRE(node->hasArgument("exists") == true);
        REQUIRE(node->hasArgument("missing") == false);
    }

    SECTION("getType Method with Default Value") {
        auto node = std::make_shared<NodeNN>();

        // Without setting type
        REQUIRE(node->getType() == "");

        // Setting type
        node->setInfo("type", std::string("NodeType"));
        REQUIRE(node->getType() == "NodeType");
    }

    SECTION("Testing with Complex Data Structures in info_") {
        auto node = std::make_shared<NodeNN>();

        // Setting a map
        std::unordered_map<std::string, int> mapData = {{"key1", 1}, {"key2", 2}};
        node->setInfo("mapKey", mapData);
        REQUIRE(node->getInfo<std::unordered_map<std::string, int>>("mapKey") == mapData);

        // Setting a custom struct
        struct CustomStruct {
            int id;
            std::string name;
            bool operator==(const CustomStruct& other) const {
                return id == other.id && name == other.name;
            }
        };

        CustomStruct customData = {1, "Test"};
        node->setInfo("structKey", customData);
        REQUIRE(node->getInfo<CustomStruct>("structKey") == customData);
    }

    SECTION("GraphNN Node Forks") {
        auto graph = std::make_shared<GraphNN>();
        auto node1 = std::make_shared<NodeNN>();
        auto node2 = std::make_shared<NodeNN>();
        auto node3 = std::make_shared<NodeNN>();

        graph->addEdge(node1, node2);
        graph->addEdge(node1, node3);

        REQUIRE(node1->getChildren().size() == 2);
        REQUIRE(node2->getParents().size() == 1);
        REQUIRE(node3->getParents().size() == 1);
    }

    SECTION("Deep Copy Without Exclusions") {
        // Create a graph with a cycle
        auto graph = std::make_shared<GraphNN>();

        auto nodeA = std::make_shared<NodeNN>();
        auto nodeB = std::make_shared<NodeNN>();
        auto nodeC = std::make_shared<NodeNN>();

        nodeA->setInfo("name", std::string("A"));
        nodeB->setInfo("name", std::string("B"));
        nodeC->setInfo("name", std::string("C"));

        graph->addEdge(nodeA, nodeB);
        graph->addEdge(nodeB, nodeC);
        graph->addEdge(nodeC, nodeA); // Creates a cycle

        // Perform deep copy without exclusions
        auto copiedGraph = graph->deepCopy({});

        // Verify that the nodes are deep copies and not the same instances
        const auto& originalNodes = graph->getNodes();
        const auto& copiedNodes = copiedGraph->getNodes();

        REQUIRE(originalNodes.size() == copiedNodes.size());

        std::unordered_set<NodeNN::Ptr> originalNodeSet(originalNodes.begin(), originalNodes.end());
        std::unordered_set<NodeNN::Ptr> copiedNodeSet(copiedNodes.begin(), copiedNodes.end());

        for (const auto& originalNode : originalNodes) {
            bool foundEquivalent = false;
            for (const auto& copiedNode : copiedNodes) {
                if (originalNode != copiedNode &&
                    originalNode->getInfo<std::string>("name") == copiedNode->getInfo<std::string>("name")) {
                    foundEquivalent = true;
                    break;
                }
            }
            REQUIRE(foundEquivalent);
        }

        // Ensure that the copied nodes are not the same instances as the original nodes
        for (const auto& node : originalNodes) {
            REQUIRE(copiedNodeSet.find(node) == copiedNodeSet.end());
        }
    }

    SECTION("Deep Copy With Exclusions") {
        // Create a graph with a cycle
        auto graph = std::make_shared<GraphNN>();

        auto nodeA = std::make_shared<NodeNN>();
        auto nodeB = std::make_shared<NodeNN>();
        auto nodeC = std::make_shared<NodeNN>();

        nodeA->setInfo("name", std::string("A"));
        nodeB->setInfo("name", std::string("B"));
        nodeC->setInfo("name", std::string("C"));

        graph->addEdge(nodeA, nodeB);
        graph->addEdge(nodeB, nodeC);
        graph->addEdge(nodeC, nodeA); // Creates a cycle

        // Nodes not to copy (nodeB)
        std::unordered_set<NodeNN::Ptr> nodesNotToCopy = { nodeB };

        // Perform deep copy with exclusions
        auto copiedGraph = graph->deepCopy(nodesNotToCopy);

        const auto& originalNodes = graph->getNodes();
        const auto& copiedNodes = copiedGraph->getNodes();

        // The copied graph should have the same number of nodes
        REQUIRE(originalNodes.size() == copiedNodes.size());

        // Verify that nodeB is the same instance in both graphs
        for (const auto& node : copiedNodes) {
            if (node->getInfo<std::string>("name") == "B") {
                REQUIRE(node == nodeB);
            } else {
                REQUIRE(node != nodeA);
                REQUIRE(node != nodeC);
            }
        }

        // Verify that the connections are maintained correctly
        // Check that nodeA's child is nodeB in both graphs
        auto copiedNodeA = *std::find_if(copiedNodes.begin(), copiedNodes.end(), [](const NodeNN::Ptr& n) {
            return n->getInfo<std::string>("name") == "A";
        });

        REQUIRE(copiedNodeA->getChildren().size() == 1);
        REQUIRE(copiedNodeA->getChildren().count(nodeB) == 1);

        // Check that nodeC's parent is nodeB in both graphs
        auto copiedNodeC = *std::find_if(copiedNodes.begin(), copiedNodes.end(), [](const NodeNN::Ptr& n) {
            return n->getInfo<std::string>("name") == "C";
        });

        REQUIRE(copiedNodeC->getParents().size() == 1);
        REQUIRE(copiedNodeC->getParents().count(nodeB) == 1);
    }

    // SECTION("Merging Nodes") {
    //     // Create a graph
    //     GraphNN graph;

    //     // Create nodes
    //     auto node1 = std::make_shared<NodeNN>();
    //     auto node2 = std::make_shared<NodeNN>();
    //     auto node3 = std::make_shared<NodeNN>();
    //     auto node4 = std::make_shared<NodeNN>();

    //     // Set some info
    //     node1->setInfo("name", std::string("Node 1"));
    //     node1->setInfo("value", 10);

    //     node2->setInfo("name", std::string("Node 2"));
    //     node2->setInfo("value", 20);

    //     node3->setInfo("name", std::string("Node 3"));
    //     node3->setInfo("value", 30);

    //     node4->setInfo("name", std::string("Node 4"));
    //     node4->setInfo("value", 40);

    //     // Build connections
    //     graph.addEdge(node1, node3);
    //     graph.addEdge(node2, node3);
    //     graph.addEdge(node3, node4);

    //     // Add nodes to the graph
    //     graph.addNode(node1);
    //     graph.addNode(node2);
    //     graph.addNode(node3);
    //     graph.addNode(node4);

    //     // Nodes to merge
    //     std::set<NodeNN::Ptr> nodesToMerge = { node1, node2 };

    //     // Merge nodes
    //     graph.mergeNodes(nodesToMerge);

    //     // The graph should now have one merged node, node3, and node4
    //     const auto& nodes = graph.getNodes();
    //     REQUIRE(nodes.size() == 3);

    //     // Find the merged node
    //     NodeNN::Ptr mergedNode = nullptr;
    //     for (const auto& node : nodes) {
    //         if (node->getInfo<int>("value", 0) == 20 ) { // Value from node2
    //             mergedNode = node;
    //             break;
    //         }
    //     }
    //     REQUIRE(mergedNode != nullptr);

    //     // Check merged info
    //     REQUIRE(mergedNode->getInfo<std::string>("name") == "Node 2");
    //     REQUIRE(mergedNode->getInfo<int>("value") == 20); // Value from node2 overwrites node1

    //     // Check connections
    //     // Merged node should have node3 as a child
    //     REQUIRE(mergedNode->getChildren().size() == 1);
    //     REQUIRE(mergedNode->getChildren().count(node3) == 1);

    //     // Node3 should have merged node as a parent
    //     REQUIRE(node3->getParents().size() == 1);
    //     REQUIRE(node3->getParents().count(mergedNode) == 1);

    //     // Node3 should have node4 as a child
    //     REQUIRE(node3->getChildren().size() == 1);
    //     REQUIRE(node3->getChildren().count(node4) == 1);
    // }


}
