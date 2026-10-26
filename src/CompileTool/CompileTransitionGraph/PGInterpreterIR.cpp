#include "CompileTool/Common/CommonKey.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp"
#include <fstream>
#include <limits>


namespace CorGIReg{

IRGenerationOptions IRGenerationOptions::read(std::istream& input) {
    IRGenerationOptions result;
    std::set<std::string> seen;
    auto trim = [](const std::string& text) {
        const auto begin = text.find_first_not_of(" \t\r\n");
        return begin == std::string::npos ? std::string{} :
            text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
    };
    auto number = [](const std::string& value, unsigned long long maximum) {
        if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
            throw std::invalid_argument("Expected an unsigned integer");
        const auto parsed = std::stoull(value);
        if (parsed > maximum) throw std::invalid_argument("Integer exceeds allowed limit");
        return parsed;
    };
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        line = trim(line.substr(0, line.find('#')));
        if (line.empty()) continue;
        try {
            const auto separator = line.find('=');
            if (separator == std::string::npos) throw std::invalid_argument("Expected key = value");
            const auto key = trim(line.substr(0, separator));
            const auto value = trim(line.substr(separator + 1));
            if (!seen.insert(key).second) throw std::invalid_argument("Duplicate key");
            if (key == "mode") {
                if (value != "random" && value != "deterministic") throw std::invalid_argument("Unknown mode");
                result.randomMissing = value == "random";
            } else if (key == "seed") {
                result.seed = static_cast<std::uint32_t>(number(value, std::numeric_limits<std::uint32_t>::max()));
            } else if (key == "max_loops") {
                result.maxLoops = static_cast<std::size_t>(number(value, 1000000));
            } else if (key == "max_nodes") {
                result.maxNodes = static_cast<std::size_t>(number(value, std::numeric_limits<std::size_t>::max()));
            } else {
                const auto id = static_cast<std::size_t>(number(key, 1000000));
                if (!id || result.repetitions.count(id)) throw std::invalid_argument("Invalid or duplicate quantifier index");
                auto& values = result.repetitions[id];
                std::size_t begin = 0;
                do {
                    const auto end = value.find(',', begin);
                    values.push_back(static_cast<std::size_t>(number(trim(value.substr(begin, end - begin)), 1000000)));
                    if (end == std::string::npos) break;
                    begin = end + 1;
                } while (true);
            }
        } catch (const std::exception& error) {
            throw std::invalid_argument("IR values line " + std::to_string(lineNumber) + ": " + error.what());
        }
    }
    if (input.bad() || (input.fail() && !input.eof())) throw std::runtime_error("Cannot read IR values");
    return result;
}

IRGenerationOptions IRGenerationOptions::fromFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open IR values: " + path);
    return read(input);
}

PGInterpreterIR::PGInterpreterIR(const std::string& expression, const std::string& valuesFile)
    : PGInterpreterIR(expression, IRGenerationOptions::fromFile(valuesFile)) {}

namespace {
using IRTree = std::shared_ptr<AstNode<tGTokensTypes>>;

// Separate expansion from parsing: each visit creates a fresh occurrence, so
// nested repetitions can consume different values without copying a template.
class ConfiguredIRGenerator {
    struct Fragment {
        std::vector<NodeNN::Ptr> first, last;
        bool common = false;
    };
    const IRGenerationOptions& options;
    std::mt19937 random;
    std::map<const AstNode<tGTokensTypes>*, std::size_t> indices;
    std::map<std::size_t, tGTokensTypes> kinds;
    std::map<std::size_t, std::size_t> consumed;
    std::map<std::string, NodeNN::Ptr> anchors;
    std::shared_ptr<GraphNN> graph = std::make_shared<GraphNN>();
    std::size_t visits = 0;
    std::map<NodeNN::Ptr, PortNumber> nextInput;
    std::map<NodeNN::Ptr, bool> automaticInputs;

    void connect(const NodeNN::Ptr& from, const NodeNN::Ptr& to, const std::string& port) {
        if (port.empty()) { graph->addEdge(from, to); return; }
        const bool automatic = port == "*";
        auto mode = automaticInputs.emplace(to, automatic);
        if (!mode.second && mode.first->second != automatic)
            throw std::invalid_argument("Cannot mix automatic and fixed input ports on one node");
        PortNumber input = automatic ? ++nextInput[to] : static_cast<PortNumber>(std::stoull(port));
        for (const auto& parent : to->getParents()) for (const auto& binding : parent->getConnectionPorts(to))
            if (binding.input == input && parent != from)
                throw std::invalid_argument("Input port has multiple producers");
        graph->addEdge(from, to, {input, std::nullopt});
    }

    void index(const IRTree& tree) {
        for (const auto& child : tree->getChilds()) index(child);
        auto type = tree->getType();
        if (type == tGTokensTypes::QOM || type == tGTokensTypes::QZM || type == tGTokensTypes::QZO) {
            auto id = indices.size() + 1;
            indices.emplace(tree.get(), id);
            kinds.emplace(id, type);
        }
    }
    std::size_t count(const IRTree& tree) {
        const auto id = indices.at(tree.get());
        const auto invocation = consumed[id]++;
        const auto found = options.repetitions.find(id);
        if (found != options.repetitions.end()) {
            const auto& values = found->second;
            if (values.size() == 1) return values.front();
            if (invocation < values.size()) return values[invocation];
            throw std::invalid_argument("Exhausted values for quantifier " + std::to_string(id));
        }
        if (!options.randomMissing)
            throw std::invalid_argument("Missing value for quantifier " + std::to_string(id));
        const auto type = tree->getType();
        return std::uniform_int_distribution<std::size_t>(type == tGTokensTypes::QOM ? 1 : 0,
            type == tGTokensTypes::QZO ? 1 : options.maxLoops + (type == tGTokensTypes::QOM ? 1 : 0))(random);
    }
    Fragment join(Fragment left, Fragment right, bool reverse, bool separate = false, const std::string& port = "") {
        if (reverse) std::swap(left, right);
        if (!port.empty() && (left.first.empty() || right.first.empty()))
            throw std::invalid_argument("A numbered connection requires two nonempty operands");
        if (left.first.empty()) return right;
        if (right.first.empty()) return left;
        if (separate) {
            left.first.insert(left.first.end(), right.first.begin(), right.first.end());
            left.last.insert(left.last.end(), right.last.begin(), right.last.end());
        } else {
            for (const auto& from : left.last) for (const auto& to : right.first)
                connect(from, to, port);
            left.last = std::move(right.last);
        }
        left.common = left.common || right.common;
        return left;
    }
    Fragment visit(const IRTree& tree) {
        // Also bounds empty nested expansions, which have no nodes to count.
        if (++visits > 1000000) throw std::length_error("IR expansion exceeds visit limit");
        const auto type = tree->getType();
        const auto& children = tree->getChilds();
        if (type == tGTokensTypes::NEXT || type == tGTokensTypes::PREC || type == tGTokensTypes::SEP) {
            auto left = visit(children.at(0));
            auto right = visit(children.at(1));
            return join(std::move(left), std::move(right), type == tGTokensTypes::PREC, type == tGTokensTypes::SEP, tree->getValue());
        }
        if (type == tGTokensTypes::LPAREN) return visit(children.at(0));
        if (type == tGTokensTypes::END) return {};
        if (type == tGTokensTypes::QOM || type == tGTokensTypes::QZM || type == tGTokensTypes::QZO) {
            const auto repetitions = count(tree);
            Fragment result;
            for (std::size_t i = 0; i < repetitions; ++i) {
                auto body = visit(children.at(0));
                const bool common = body.common;
                result = join(std::move(result), std::move(body), false, common);
            }
            return result;
        }
        const bool common = type == tGTokensTypes::CKEY || type == tGTokensTypes::SCKEY;
        if (type != tGTokensTypes::KEY && !common) throw std::logic_error("Unsupported IR node");
        std::string key = tree->getValue(), predicate = key;
        if (common) {
            auto parsed = parseCommonKey(key);
            key = parsed.identity(); predicate = parsed.predicate;
            auto found = anchors.find(key);
            if (found != anchors.end()) return {{found->second}, {found->second}, true};
        }
        if (graph->getNodes().size() >= options.maxNodes) throw std::length_error("IR exceeds maxNodes");
        auto node = std::make_shared<NodeNN>();
        node->setInfo("Type", predicate);
        node->setInfo("Key", key);
        if (common) { node->setInfo("isCommon", true); anchors.emplace(key, node); }
        graph->addNode(node);
        return {{node}, {node}, common};
    }
public:
    explicit ConfiguredIRGenerator(const IRGenerationOptions& value) : options(value), random(value.seed) {}
    std::shared_ptr<GraphNN> run(const IRTree& tree) {
        index(tree);
        if (options.maxLoops > 1000000) throw std::invalid_argument("maxLoops exceeds expansion limit");
        for (const auto& item : options.repetitions) {
            if (!kinds.count(item.first) || item.second.empty())
                throw std::invalid_argument("Unknown or empty quantifier " + std::to_string(item.first));
            for (auto value : item.second) {
                auto type = kinds.at(item.first);
                if (value > 1000000 || (type == tGTokensTypes::QOM && value == 0) ||
                    (type == tGTokensTypes::QZO && value > 1))
                    throw std::invalid_argument("Invalid value for quantifier " + std::to_string(item.first));
            }
        }
        visit(tree);
        for (const auto& item : options.repetitions)
            if (item.second.size() > 1 && consumed[item.first] != item.second.size())
                throw std::invalid_argument("Unused values for quantifier " + std::to_string(item.first));
        return graph;
    }
};
}

PGInterpreterIR::PGInterpreterIR(const std::string& expression, IRGenerationOptions options)
    : mParser(expression), mActGroupe(0), mGraphMatchExpr(expression),
      mOptions(std::move(options)), mConfigured(true) {}

PGInterpreterIR::PGInterpreterIR(const std::string graphMatchExpr)
    : PGInterpreterIR(graphMatchExpr, 10) {}

PGInterpreterIR::PGInterpreterIR(const std::string graphMatchExpr, size_t maxLoops)
    : mMaxLoops(maxLoops), mParser(graphMatchExpr){
    mActGroupe = 0;
}
 

std::shared_ptr<GraphNN>  PGInterpreterIR::interpret(void){
    mActGroupe = 0;
    mCommonNodes.clear();
    std::shared_ptr<AstNode<tGTokensTypes>> tree = mParser.parse();
    if (mConfigured) return ConfiguredIRGenerator(mOptions).run(tree);
    std::function<bool(const IRTree&)> hasPorts = [&](const IRTree& node) {
        if ((node->getType() == tGTokensTypes::NEXT || node->getType() == tGTokensTypes::PREC) && !node->getValue().empty()) return true;
        for (const auto& child : node->getChilds()) if (hasPorts(child)) return true;
        return false;
    };
    if (hasPorts(tree)) {
        IRGenerationOptions options;
        options.randomMissing = true; options.maxLoops = mMaxLoops;
        return ConfiguredIRGenerator(options).run(tree);
    }
    std::shared_ptr<GraphNN> out = visit(tree);

    // combine the common nodes
    for (auto common : mCommonNodes){   
        out->mergeNodes(common.second);
    }

    return out;
}



std::shared_ptr<GraphNN> PGInterpreterIR::unionIR(std::shared_ptr<GraphNN>& leftTG,std::shared_ptr<GraphNN>& rightTG){
    for (auto rNode : rightTG->getNodes()){
        leftTG->addNode(rNode);
    }
    return leftTG;
}

void PGInterpreterIR::deleteIR(std::shared_ptr<GraphNN>& tg){

    // Iterate over all nodes in the task graph 'tg'
    for (auto& node : tg->getNodes()) {
        // Retrieve the key from the node's information
        auto nodeKey = node->getInfo<std::string>("Key");

        // Find the key in the map
        auto mapIt = mCommonNodes.find(nodeKey);
        if (mapIt != mCommonNodes.end()) {
            // We found the key, now remove the node from the corresponding set
            auto& nodeSet = mapIt->second;
            auto setIt = nodeSet.find(node);
            if (setIt != nodeSet.end()) {
                nodeSet.erase(setIt); // Safely erase using the iterator
            }
        }

        // Reset the smart pointer if needed
        //node.reset(); 
    }
    //tg.reset();
}

std::shared_ptr<GraphNN> PGInterpreterIR::visit(std::shared_ptr<AstNode<tGTokensTypes>> AstTree){

    std::vector<std::shared_ptr<AstNode<tGTokensTypes>>> nextAstNodes = AstTree->getChilds();

    if(AstTree->getType() == tGTokensTypes::SEP){
        return sepF(visit(nextAstNodes[0]),visit(nextAstNodes[1]));
    }else if(AstTree->getType() == tGTokensTypes::NEXT){
        return nextF(visit(nextAstNodes[0]),visit(nextAstNodes[1]));
    }else if(AstTree->getType() == tGTokensTypes::PREC){
        return precF(visit(nextAstNodes[0]),visit(nextAstNodes[1]));
    }else if(AstTree->getType() == tGTokensTypes::QOM){
        return qomF(visit(nextAstNodes[0]));
    }else if(AstTree->getType() == tGTokensTypes::QZM){
        return qzmF(visit(nextAstNodes[0]));
    }else if(AstTree->getType() == tGTokensTypes::QZO){
        return qzoF(visit(nextAstNodes[0]));
    }else if(AstTree->getType() == tGTokensTypes::KEY || AstTree->getType() == tGTokensTypes::CKEY || AstTree->getType() == tGTokensTypes::SCKEY){
        return keyF(AstTree);
    }else if (AstTree->getType() == tGTokensTypes::END){
        return endF(AstTree);
    }else if(AstTree->getType() == tGTokensTypes::LPAREN){
        mActGroupe += 1;
        std::shared_ptr<GraphNN> out = visit(nextAstNodes[0]);
        mActGroupe -= 1;
        return out;
    }else{
        throw std::logic_error("visit Bad token type" );
    }
}


std::shared_ptr<GraphNN> PGInterpreterIR::keyF(std::shared_ptr<AstNode<tGTokensTypes>> AstNode){
    /*leaf 
        K -> V
    */

    auto graph =  std::make_shared<GraphNN>();
    std::shared_ptr<NodeNN> node;
    std::string lexeme = AstNode->getValue();
    node = std::make_shared<NodeNN>();
    /*
    the key in the query
    */
    node->setInfo("Key",lexeme);

    if (AstNode->getType() == tGTokensTypes::KEY){
        node->setInfo("Type",lexeme);
    }else if(AstNode->getType() == tGTokensTypes::CKEY || AstNode->getType() == tGTokensTypes::SCKEY ){
        node->setInfo("isCommon",true);

        const auto key = parseCommonKey(lexeme);
        node->setInfo("Type", key.predicate);
        node->setInfo("Key", key.identity());
        mCommonNodes[key.identity()].insert(node);

   
    }else{

        throw std::logic_error("keyF Bad in AST" );
    }
    
    graph->addNode(node);
    return graph;

}

std::shared_ptr<GraphNN> PGInterpreterIR::endF(std::shared_ptr<AstNode<tGTokensTypes>> AstNode){
    /*
        Null
        S -> V
    */
    return std::make_shared<GraphNN>();
}

std::shared_ptr<GraphNN> PGInterpreterIR::sepF(std::shared_ptr<GraphNN> leftTG,std::shared_ptr<GraphNN> rightTG){
    for (auto rNode : rightTG->getNodes()){
            leftTG->addNode(rNode);
    }

    
    return leftTG;
}

std::shared_ptr<GraphNN> PGInterpreterIR::nextF(std::shared_ptr<GraphNN> leftTG,std::shared_ptr<GraphNN> rightTG){
    /*
        combine the 2 Graph
        get the node that have no child in leftTG
        and connect to the node with no parent in rightTG
    */


    for (auto lNode : leftTG->getNodes()){
        for (auto rNode : rightTG->getNodes()){
            if (rNode->getParents().size() == 0 && 
                lNode->getChildren().size() == 0){
                leftTG->addEdge(lNode,rNode);
            }
        }
    }

    for (auto rNode : rightTG->getNodes()){
        leftTG->addNode(rNode);
    }
    rightTG = unionIR(rightTG,leftTG);

    return leftTG;
}

std::shared_ptr<GraphNN> PGInterpreterIR::precF(std::shared_ptr<GraphNN> leftTG,std::shared_ptr<GraphNN> rightTG){
    /*
        combine the 2 Graph
        all valid node of A are  merge with Start B, Start B is un Start
        update the relative reference  

           A          B
        SA -> VA + SB -> VB
           B    A
        SB -x> q -x> VA
    */
    for (auto rNode : rightTG->getNodes()){
        leftTG->addNode(rNode);
   }

    for (auto lNode : leftTG->getNodes()){
        for (auto rNode : rightTG->getNodes()){
            if (rNode->getParents().size() == 0 && 
                lNode->getChildren().size() == 0){
                leftTG->addEdge(rNode,lNode);
            }
        }
    }

    return leftTG;
}

std::shared_ptr<GraphNN> PGInterpreterIR::qomF(std::shared_ptr<GraphNN> tg){
    /*
        +
        valid node is connect to the child of Start with the same edge condition
            A
        S -> V

            A
        S -> V
          (E|R)
        V -> S
    */
   //todo
    bool hasCommon = false;
    for (auto node : tg->getNodes()){
        if (node->hasArgument("isCommon")){
            hasCommon = true;
            break;
        }
    }

    auto qzmfPart = qzmF(tg);

    // qzmF removes the template from the common-node registry. For '+',
    // that template is the mandatory occurrence and remains in the result.
    for (const auto& node : tg->getNodes()) {
        if (node->hasArgument("isCommon")) {
            mCommonNodes[node->getInfo<std::string>("Key")].insert(node);
        }
    }

    if (!hasCommon){
        return nextF(tg,qzmfPart);
    }
    return unionIR(tg,qzmfPart); 

}

std::shared_ptr<GraphNN> PGInterpreterIR::qzmF(std::shared_ptr<GraphNN> tg){
    /*
    have zero of more instance of the graph, is like dublicate the graph 
    and combine it a rdm between 0 and N time the graph
    */
    

    std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));
    // Define the maximum value for N as size_t
    // Create a distribution from 0 to N (inclusive), using size_t
    std::uniform_int_distribution<size_t> dist(0, mMaxLoops);
    // Generate a random number of iterations
    size_t randomIterations = dist(rng);



    std::shared_ptr<GraphNN> newTg = std::make_shared<GraphNN>();

    for (size_t i = 0; i < randomIterations; i++) {
        std::shared_ptr<GraphNN> tgFixed = tg->deepCopy({});
        /*
        add the common nodes to the list of common nodes
        */
        bool hasCommon = false;
        for (auto node : tgFixed->getNodes()){
            if (node->hasArgument("isCommon")){
                const std::string key = node->getInfo<std::string>("Key");
                hasCommon = true;
                if (mCommonNodes.find(key) == mCommonNodes.end()){
                    mCommonNodes[key] = std::set<std::shared_ptr<NodeNN>>({node});
                }else{
                    mCommonNodes[key].insert(node);
                }
            }
        }

        if (!hasCommon){
            //it's a sequence of nodes just concat it 
            newTg = nextF(newTg,tgFixed);
        }else{
            newTg = unionIR(newTg,tgFixed); 
        }
    }
        
    /* it's important to delete the old graph because of the common nodes save in the map*/
    deleteIR( tg);

    return newTg;

}

std::shared_ptr<GraphNN> PGInterpreterIR::qzoF(std::shared_ptr<GraphNN> tg){
    /*
    quantification zero or one
    just connect the start to the valid
     */

    std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));

    // Create a distribution that covers only 0 and 1
    std::uniform_int_distribution<int> dist(0, 1);

    // Generate a random binary value
    int randomBinary = dist(rng);

    if (randomBinary == 0) {
        return tg;

    } else {
        deleteIR(tg);
        return std::make_shared<GraphNN>();

    }
}


}//namespace CorGIReg
