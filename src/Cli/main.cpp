#include "Cli/MatchRequest.hpp"
#include <iostream>
#include <exception>

int main() {
    try { CorGIReg::matchRequest(std::cin, std::cout); return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}
