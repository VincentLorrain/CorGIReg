#pragma once
#include <iosfwd>
#include <cstddef>

namespace CorGIReg {
// Internal versioned stdin protocol. The public CLI owns file decoding.
// Compilation and rule evaluation remain serial; jobs controls tuple traversal.
void matchRequest(std::istream& input, std::ostream& output, std::size_t jobs = 1);
}
