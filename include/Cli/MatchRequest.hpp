#pragma once
#include "NodeTest/NodeLibrary.hpp"
#include <iosfwd>
#include <cstddef>
#include <string>

namespace CorGIReg {
// Internal versioned stdin protocol. The public CLI owns file decoding.
// Compilation and rule evaluation remain serial; jobs controls tuple traversal.
void matchRequest(std::istream& input, std::ostream& output, std::size_t jobs = 1);
// With a native library, the request carries node documents (version 3 or 6)
// and no definitions: the library resolves them for `format`, and the reply
// contains the public node_definitions report instead of definition_nodes.
void matchRequest(std::istream& input, std::ostream& output, std::size_t jobs,
                  const NodeLibrary* library, const std::string& format);
// Validated definitions as JSON: name -> schema_version, name, description,
// formats, schema 4 key_matching, and absolute source file.
void writeNodeLibrary(std::ostream& output, const NodeLibrary& library);
}
