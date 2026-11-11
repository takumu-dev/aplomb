// Logs a message of each severity to the default sink, which the test reads from stderr.
#include <aplomb/core/log_sink.hpp>

int main() {
    using enum aplomb::LogSeverity;
    aplomb::log(trace, "first");
    aplomb::log(debug, "second");
    aplomb::log(info, "third");
    aplomb::log(warning, "fourth");
    aplomb::log(error, "fifth");
}
