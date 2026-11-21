#include <aplomb/core/log_sink.hpp>

#include <atomic>
#include <cstdio>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace aplomb {

namespace {

// The sink is a process-wide hook, which a message from anywhere must reach.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): a process-wide hook
constinit std::atomic<LogSink*> current_sink{nullptr};

std::string_view severity_name(LogSeverity severity) noexcept {
    switch (severity) {
    case LogSeverity::trace:
        return "trace";
    case LogSeverity::debug:
        return "debug";
    case LogSeverity::info:
        return "info";
    case LogSeverity::warning:
        return "warning";
    case LogSeverity::error:
        return "error";
    }
    std::unreachable();
}

// The default sink. Each message goes out in one write, so that lines logged from several
// threads at once don't interleave.
void write_to_stderr(LogSeverity severity, std::string_view message) {
    const std::string line = std::format("{}: {}\n", severity_name(severity), message);
    std::fwrite(line.data(), 1, line.size(), stderr);
}

} // namespace

void set_log_sink(LogSink* sink) noexcept { current_sink.store(sink, std::memory_order_release); }

void log(LogSeverity severity, std::string_view message) {
    if (LogSink* const sink = current_sink.load(std::memory_order_acquire); sink != nullptr) {
        sink->write(severity, message);
    } else {
        write_to_stderr(severity, message);
    }
}

} // namespace aplomb
