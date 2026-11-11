#pragma once

#include <aplomb/core/log_sink.hpp>

/// Routes aplomb's diagnostics to a sink for the length of a test.
class ScopedLogSink {
  public:
    explicit ScopedLogSink(aplomb::LogSink& sink) noexcept { aplomb::set_log_sink(&sink); }

    ScopedLogSink(const ScopedLogSink&) = delete;
    ScopedLogSink(ScopedLogSink&&) = delete;
    ScopedLogSink& operator=(const ScopedLogSink&) = delete;
    ScopedLogSink& operator=(ScopedLogSink&&) = delete;

    ~ScopedLogSink() { aplomb::set_log_sink(nullptr); }
};
