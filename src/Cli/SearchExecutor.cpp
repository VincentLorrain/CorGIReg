#include "Cli/SearchExecutor.hpp"
#include <algorithm>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace CorGIReg {
void runOrderedSearch(std::uint64_t count, std::size_t jobs,
                      const std::function<std::string(std::uint64_t)>& evaluate,
                      const std::function<void(const std::string&)>& consume) {
    if (jobs == 0 || jobs > 256) throw std::invalid_argument("Jobs must be between 1 and 256");
    if (count == 0) return;
    jobs = static_cast<std::size_t>(std::min<std::uint64_t>(jobs, count));
    if (jobs == 1) {
        for (std::uint64_t i = 0; i < count; ++i) consume(evaluate(i));
        return;
    }
    struct Slot { std::string value; bool ready = false; };
    std::vector<Slot> slots(static_cast<std::size_t>(std::min<std::uint64_t>(count, jobs * 8)));
    std::mutex mutex;
    std::condition_variable changed;
    std::uint64_t next = 0, consumed = 0;
    bool stop = false;
    std::exception_ptr failure;
    std::vector<std::thread> workers;
    workers.reserve(jobs);
    auto worker = [&] {
        try {
            while (true) {
                std::uint64_t index;
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    changed.wait(lock, [&] { return stop || next == count || next - consumed < slots.size(); });
                    if (stop || next == count) return;
                    index = next++;
                }
                auto value = evaluate(index);
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    if (stop) return;
                    auto& slot = slots[index % slots.size()];
                    slot.value = std::move(value);
                    slot.ready = true;
                }
                changed.notify_all();
            }
        } catch (...) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (!failure) failure = std::current_exception();
                stop = true;
            }
            changed.notify_all();
        }
    };
    auto join = [&] {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stop = true;
        }
        changed.notify_all();
        for (auto& thread : workers) thread.join();
    };
    try {
        for (std::size_t i = 0; i < jobs; ++i) workers.emplace_back(worker);
        for (std::uint64_t i = 0; i < count; ++i) {
            std::string value;
            {
                std::unique_lock<std::mutex> lock(mutex);
                changed.wait(lock, [&] { return failure || slots[i % slots.size()].ready; });
                if (failure) std::rethrow_exception(failure);
                auto& slot = slots[i % slots.size()];
                value = std::move(slot.value);
                slot.ready = false;
                ++consumed;
            }
            changed.notify_all();
            consume(value);
        }
    } catch (...) {
        join();
        throw;
    }
    join();
}
}
