#pragma once

#include <aplomb/core/log_sink.hpp>

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

/// One message a log sink received.
struct LoggedMessage {
    aplomb::LogSeverity severity;
    std::string text;

    bool operator==(const LoggedMessage&) const = default;
};

/// Keeps every message it receives, from any thread.
class RecordingLogSink final : public aplomb::LogSink {
  public:
    void write(aplomb::LogSeverity severity, std::string_view message) override {
        const std::scoped_lock lock(mutex_);
        messages_.push_back({.severity = severity, .text = std::string(message)});
    }

    std::vector<LoggedMessage> messages() const {
        const std::scoped_lock lock(mutex_);
        return messages_;
    }

  private:
    mutable std::mutex mutex_;
    std::vector<LoggedMessage> messages_;
};
