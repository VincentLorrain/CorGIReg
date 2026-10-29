#include "Cli/MatchRequest.hpp"
#include <iostream>
#include <exception>
#include <string>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        const std::string usage = "Usage: corgireg-match [--jobs 1..256] [--node-library PATH]... "
                                  "[--format NAME] [--list-definitions]";
        std::size_t jobs = 1;
        std::vector<std::filesystem::path> libraries;
        std::string format = "json";
        bool list = false;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--list-definitions") { list = true; continue; }
            if (i + 1 == argc) throw std::invalid_argument(usage);
            const std::string value = argv[++i];
            if (option == "--jobs") {
                if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
                    throw std::invalid_argument("Jobs must be between 1 and 256");
                const auto parsed = std::stoull(value);
                if (parsed == 0 || parsed > 256)
                    throw std::invalid_argument("Jobs must be between 1 and 256");
                jobs = static_cast<std::size_t>(parsed);
            }
            else if (option == "--node-library") libraries.emplace_back(value);
            else if (option == "--format" && !value.empty()) format = value;
            else throw std::invalid_argument(usage);
        }
        const auto library = CorGIReg::NodeLibrary::load(libraries);
        if (list) {
            if (libraries.empty()) throw std::invalid_argument("--list-definitions requires --node-library");
            CorGIReg::writeNodeLibrary(std::cout, library);
            return 0;
        }
        CorGIReg::matchRequest(std::cin, std::cout, jobs, libraries.empty() ? nullptr : &library, format);
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}
