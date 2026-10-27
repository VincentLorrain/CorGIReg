#include "CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp"
#include "GraphAdapters/GraphJson.hpp"
#include <fstream>
#include <iostream>
#include <limits>

using namespace CorGIReg;
int main(int argc, char** argv) {
    try {
        std::string query, file, destination = "-";
        std::optional<std::uint32_t> seed;
        std::optional<std::size_t> loops;
        bool random = false;
        auto number = [](const std::string& text, std::uint64_t maximum) {
            if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
                throw std::invalid_argument("Expected an unsigned integer");
            const auto value = std::stoull(text);
            if (value > maximum) throw std::invalid_argument("Number exceeds allowed range");
            return value;
        };
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--help" || option == "-h") {
                std::cout << "corgireg-generate -r PATTERN [-v VALUES_FILE] [-o GRAPH_JSON]\n"
                             "  [--random] [--seed N] [--max-loops N]\n"
                             "Default: deterministic values, JSON on stdout. Ports: A-1->B; A-*->B equals A->B.\n";
                return 0;
            }
            if (option == "--random") { random = true; continue; }
            if (i + 1 == argc) throw std::invalid_argument("Missing option value: " + option);
            const std::string value = argv[++i];
            if (option == "-r" || option == "--regex") query = value;
            else if (option == "-v" || option == "--values") file = value;
            else if (option == "-o" || option == "--output") destination = value;
            else if (option == "--seed") seed = static_cast<std::uint32_t>(number(value, std::numeric_limits<std::uint32_t>::max()));
            else if (option == "--max-loops") loops = static_cast<std::size_t>(number(value, 1000000));
            else throw std::invalid_argument("Unknown option: " + option);
        }
        if (query.empty()) throw std::invalid_argument("A pattern is required (-r)");
        auto options = file.empty() ? IRGenerationOptions{} : IRGenerationOptions::fromFile(file);
        if (random) options.randomMissing = true;
        if (seed) options.seed = *seed;
        if (loops) options.maxLoops = *loops;
        auto graph = PGInterpreterIR(query, options).interpret();
        std::ostringstream serialized;
        writeGraphJson(*graph, serialized);
        // Generation and validation finish before opening a destination file.
        if (destination == "-") std::cout << serialized.str();
        else {
            std::ofstream output(destination, std::ios::binary);
            output << serialized.str(); output.close();
            if (!output) throw std::runtime_error("Cannot write output: " + destination);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "corgireg-generate: " << error.what() << '\n';
        return 2;
    }
}
