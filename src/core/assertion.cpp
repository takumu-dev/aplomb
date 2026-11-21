#include <aplomb/core/assertion.hpp>

#include <aplomb/core/log_sink.hpp>

#include <atomic>
#include <cstdlib>
#include <format>

namespace aplomb {

namespace {

// The handler is a process-wide hook, which a failed check anywhere must reach.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): a process-wide hook
constinit std::atomic<AssertionHandler> current_handler{nullptr};

} // namespace

void set_assertion_handler(AssertionHandler handler) noexcept {
    current_handler.store(handler, std::memory_order_release);
}

namespace detail {

void fail_assertion(const AssertionFailure& failure) {
    log(LogSeverity::error,
        std::format("{}:{}: assertion failed: {} ({})", failure.location.file_name(),
                    failure.location.line(), failure.message, failure.expression));
    if (const AssertionHandler handler = current_handler.load(std::memory_order_acquire);
        handler != nullptr) {
        handler(failure);
    }
    std::abort();
}

} // namespace detail

} // namespace aplomb
