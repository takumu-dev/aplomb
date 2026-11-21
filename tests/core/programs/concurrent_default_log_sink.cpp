// Logs to the default sink from several threads at once, with stderr sent to the file named by the
// one argument, then checks that every line arrived exactly once and whole.
#include <aplomb/core/log_sink.hpp>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <fstream>
#include <print>
#include <set>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int thread_count = 8;
constexpr int messages_per_thread = 500;

std::string message(int thread, int index) {
    return std::format("thread {} message {}", thread, index);
}

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): nothing is caught, so a failure ends the program.
int main(int argc, char** argv) {
    const std::span arguments(argv, static_cast<std::size_t>(argc));
    if (arguments.size() != 2) {
        std::println("usage: concurrent_default_log_sink <file for stderr>");
        return EXIT_FAILURE;
    }
    const char* const path = arguments.back();
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): stderr goes on owning its stream.
    if (std::freopen(path, "w", stderr) == nullptr) {
        std::println("can't send stderr to {}", path);
        return EXIT_FAILURE;
    }

    {
        std::vector<std::jthread> threads;
        threads.reserve(thread_count);
        for (int thread = 0; thread < thread_count; ++thread) {
            threads.emplace_back([thread] {
                for (int index = 0; index < messages_per_thread; ++index) {
                    aplomb::log(aplomb::LogSeverity::info, message(thread, index));
                }
            });
        }
    }
    std::fflush(stderr);

    std::set<std::string> missing;
    for (int thread = 0; thread < thread_count; ++thread) {
        for (int index = 0; index < messages_per_thread; ++index) {
            missing.insert(std::format("info: {}", message(thread, index)));
        }
    }
    std::ifstream written(path);
    for (std::string line; std::getline(written, line);) {
        if (missing.erase(line) == 0) {
            std::println("a line arrived broken or twice: {}", line);
            return EXIT_FAILURE;
        }
    }
    if (!missing.empty()) {
        std::println("{} lines never arrived, such as: {}", missing.size(), *missing.begin());
        return EXIT_FAILURE;
    }
}
