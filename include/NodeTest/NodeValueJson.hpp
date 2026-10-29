#pragma once

#include "NodeTest/NodeRule.hpp"
#include <ostream>
#include <string>

namespace CorGIReg {

// Strict RFC 8259 reader for node documents and node-definition files.
// Integers without fraction or exponent become uint64 (non-negative) or int64;
// other numbers become finite doubles. A leading UTF-8 BOM is ignored.
// Duplicate object keys, invalid UTF-8, non-finite numbers, integers outside
// 64 bits and nesting deeper than 64 are rejected with a line and column.
NodeValue parseJson(const std::string& text);

// Compact JSON output. Missing and Unsupported values cannot be serialized.
// Doubles use the shortest round-trip form and always keep a fraction or exponent.
void writeJson(std::ostream& output, const NodeValue& value);

}
