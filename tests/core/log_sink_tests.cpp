#include <aplomb/core/log_sink.hpp>

#include "recording_log_sink.hpp"
#include "scoped_log_sink.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

TEST_CASE("a replaced log sink receives each message with its severity", "[core]") {
    using enum aplomb::LogSeverity;
    RecordingLogSink sink;
    const ScopedLogSink scope(sink);

    aplomb::log(trace, "first");
    aplomb::log(debug, "second");
    aplomb::log(info, "third");
    aplomb::log(warning, "fourth");
    aplomb::log(error, "fifth");

    CHECK(sink.messages() == std::vector<LoggedMessage>{
                                 {.severity = trace, .text = "first"},
                                 {.severity = debug, .text = "second"},
                                 {.severity = info, .text = "third"},
                                 {.severity = warning, .text = "fourth"},
                                 {.severity = error, .text = "fifth"},
                             });
}
