// Fails a check under a replaced assertion handler. The handler ends the program successfully only
// if it receives the failure as written, after the failure was logged as an error.
#include <aplomb/core/assertion.hpp>
#include <aplomb/core/log_sink.hpp>

#include <cstdint>
#include <cstdlib>
#include <format>
#include <print>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

namespace {

// A handler is a plain function and can't capture, so what it compares lives here.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): the handler reads it.
std::uint_least32_t failing_line = 0;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): the handler reads it.
std::vector<std::string> logged_errors;

class ErrorSink final : public aplomb::LogSink {
  public:
    void write(aplomb::LogSeverity severity, std::string_view message) override {
        if (severity == aplomb::LogSeverity::error) {
            logged_errors.emplace_back(message);
        }
    }
};

[[noreturn]] void check_failure(const aplomb::AssertionFailure& failure) {
    const std::string_view file = std::source_location::current().file_name();
    const std::vector<std::string> expected_errors{
        std::format("{}:{}: assertion failed: a square has four sides (sides == 4)", file,
                    failing_line),
    };
    const bool as_written =
        failure.expression == "sides == 4" && failure.message == "a square has four sides" &&
        failure.location.file_name() == file && failure.location.line() == failing_line;
    if (!as_written || logged_errors != expected_errors) {
        std::println(stderr, "received: {} ({}) at {}:{}", failure.message, failure.expression,
                     failure.location.file_name(), failure.location.line());
        std::println(stderr, "logged: {}", logged_errors);
        std::_Exit(EXIT_FAILURE);
    }
    std::_Exit(EXIT_SUCCESS);
}

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): nothing is caught, so a failure ends the program.
int main() {
    ErrorSink sink;
    aplomb::set_log_sink(&sink);
    aplomb::set_assertion_handler(check_failure);

    const int sides = 3;
    failing_line = std::source_location::current().line() + 1;
    APLOMB_ASSERT(sides == 4, "a square has four sides");

    std::println(stderr, "the check didn't fail");
    return EXIT_FAILURE;
}
