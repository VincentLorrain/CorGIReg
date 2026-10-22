#pragma once
#include <iosfwd>

namespace CorGIReg {
// Internal versioned stdin protocol. The public CLI owns file decoding.
void matchRequest(std::istream& input, std::ostream& output);
}
