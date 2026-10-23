#include "Cli/SearchExecutor.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace CorGIReg;

TEST_CASE("Ordered search runs concurrently and preserves every tuple", "[cli][parallel]") {
    std::mutex mutex;
    std::condition_variable started;
    std::set<std::thread::id> threads;
    std::vector<unsigned> visits(137);
    std::vector<std::string> results;
    const auto caller = std::this_thread::get_id();
    runOrderedSearch(visits.size(), 4, [&](std::uint64_t i) {
        std::unique_lock<std::mutex> lock(mutex);
        threads.insert(std::this_thread::get_id());
        ++visits[i];
        started.notify_all();
        // A bounded rendezvous proves overlapping workers without asserting speed.
        started.wait_for(lock, std::chrono::seconds(3), [&] { return threads.size() >= 2; });
        return std::to_string(i);
    }, [&](const std::string& value) {
        REQUIRE(std::this_thread::get_id() == caller);
        results.push_back(value);
    });
    REQUIRE(threads.size() >= 2);
    REQUIRE(threads.size() <= 4);
    REQUIRE(results.size() == visits.size());
    for (std::size_t i = 0; i < visits.size(); ++i) {
        REQUIRE(visits[i] == 1);
        REQUIRE(results[i] == std::to_string(i));
    }
}

TEST_CASE("Ordered search joins workers on evaluation and output failures", "[cli][parallel]") {
    for (std::size_t jobs : {1, 4}) {
        INFO("jobs=" << jobs);
        std::atomic<unsigned> active{0};
        auto evaluate = [&](std::uint64_t i) {
            struct Guard {
                std::atomic<unsigned>& active;
                Guard(std::atomic<unsigned>& a) : active(a) { ++active; }
                ~Guard() { --active; }
            } guard(active);
            if (i == 9) throw std::runtime_error("tuple failed");
            return std::to_string(i);
        };
        REQUIRE_THROWS_WITH(runOrderedSearch(137, jobs, evaluate, [](const auto&) {}), "tuple failed");
        REQUIRE(active == 0);
        REQUIRE_THROWS_WITH(runOrderedSearch(137, jobs,
            [](auto i) { return std::to_string(i); },
            [](const auto&) { throw std::runtime_error("output failed"); }), "output failed");
        std::vector<std::string> results;
        runOrderedSearch(3, jobs, [](auto i) { return std::to_string(i); },
                         [&](const auto& value) { results.push_back(value); });
        REQUIRE(results == std::vector<std::string>{"0", "1", "2"});
    }
}

TEST_CASE("Ordered search validates worker limits and empty input", "[cli][parallel]") {
    unsigned calls = 0;
    auto evaluate = [&](auto) { ++calls; return std::string{}; };
    auto consume = [](const auto&) {};
    REQUIRE_THROWS_AS(runOrderedSearch(1, 0, evaluate, consume), std::invalid_argument);
    REQUIRE_THROWS_AS(runOrderedSearch(1, 257, evaluate, consume), std::invalid_argument);
    runOrderedSearch(0, 4, evaluate, consume);
    REQUIRE(calls == 0);
    runOrderedSearch(1, 256, evaluate, consume);
    REQUIRE(calls == 1);
}
