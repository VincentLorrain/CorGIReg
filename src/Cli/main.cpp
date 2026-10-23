#include "Cli/MatchRequest.hpp"
#include <iostream>
#include <exception>
#include <string>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        std::size_t jobs = 1;
        if (argc != 1) {
            if (argc != 3 || std::string(argv[1]) != "--jobs")
                throw std::invalid_argument("Usage: corgireg-match [--jobs 1..256]");
            const std::string value = argv[2];
            if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
                throw std::invalid_argument("Jobs must be between 1 and 256");
            const auto parsed = std::stoull(value);
            if (parsed == 0 || parsed > 256)
                throw std::invalid_argument("Jobs must be between 1 and 256");
            jobs = static_cast<std::size_t>(parsed);
        }
        CorGIReg::matchRequest(std::cin, std::cout, jobs);
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}
