#pragma once
#include <cstdint>
#include <optional>
#include <tuple>
#include <stdexcept>

namespace CorGIReg {
using PortNumber = std::uint32_t;
struct EdgePorts {
    std::optional<PortNumber> input;
    std::optional<PortNumber> output;
    void validate() const {
        if ((input && !*input) || (output && !*output))
            throw std::invalid_argument("Port numbers start at 1");
    }
    bool operator<(const EdgePorts& other) const {
        return std::tie(input, output) < std::tie(other.input, other.output);
    }
    bool operator==(const EdgePorts& other) const {
        return input == other.input && output == other.output;
    }
};
}
