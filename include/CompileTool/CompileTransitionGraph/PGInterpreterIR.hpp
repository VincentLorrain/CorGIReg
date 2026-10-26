//PGInterpreterTG

#pragma once



#include <string>
#include <vector>
#include <map>
#include <memory>
#include <random>
#include <ctime>
#include <regex>
#include <cstdint>
#include <istream>

#include "BasicGraphIr/Graph.hpp"
#include "CompileTool/Common/AstNode.hpp"
#include "NodeTest/NodeCondition.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphLexer.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphParser.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphTokens.hpp"

namespace CorGIReg {

    struct IRGenerationOptions {
        // One-based lexical quantifier index. A singleton repeats at every invocation;
        // a longer vector supplies successive invocations of a nested quantifier.
        std::map<std::size_t, std::vector<std::size_t>> repetitions;
        bool randomMissing = false;
        std::uint32_t seed = 5489u;
        std::size_t maxLoops = 10;
        std::size_t maxNodes = 100000;
        static IRGenerationOptions read(std::istream& input);
        static IRGenerationOptions fromFile(const std::string& path);
    };

    class PGInterpreterIR {
        PartitionGraphParser mParser;
        IRGenerationOptions mOptions;
    public:
        explicit PGInterpreterIR(const std::string graphMatchExpr);
        PGInterpreterIR(const std::string graphMatchExpr, size_t maxLoops);
        PGInterpreterIR(const std::string& graphMatchExpr, IRGenerationOptions options);
        PGInterpreterIR(const std::string& graphMatchExpr, const std::string& valuesFile);
        virtual ~PGInterpreterIR() = default;
        std::shared_ptr<GraphNN> interpret();
    };
}
