#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace CorGIReg {
// Evaluate independent starting tuples concurrently, consume on the calling
// thread in index order. Callbacks must not share mutable evaluation state.
// At most 8 * jobs completed/in-flight tuple results are buffered (not bytes).
// Exceptions cancel pending work, join workers and propagate to the caller.
void runOrderedSearch(std::uint64_t count, std::size_t jobs,
                      const std::function<std::string(std::uint64_t)>& evaluate,
                      const std::function<void(const std::string&)>& consume);
}
