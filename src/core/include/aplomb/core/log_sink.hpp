/// \file
/// The log sink, which receives the diagnostics aplomb doesn't return.
///
/// Failures that correct code can meet come back as values. What remains, such as the report of a
/// failed check, goes to one process-wide sink. By default it writes to stderr; a host can route it
/// into its own log instead.

#pragma once

#include <cstdint>
#include <string_view>

namespace aplomb {

/// How severe a logged message is, from the least to the most.
enum class LogSeverity : std::uint8_t {
    trace,   ///< Detail for following aplomb's work step by step.
    debug,   ///< Detail for finding the cause of a problem.
    info,    ///< A notable event in normal running.
    warning, ///< Something wrong that aplomb carries on from.
    error,   ///< Something wrong that is a bug, such as a failed check.
};

/// Receives aplomb's log messages.
///
/// A host derives from it to send the messages to its own log, and installs an instance with
/// `set_log_sink`. Messages may arrive from any thread, and from several at once.
class LogSink {
  public:
    /// Destroys the sink.
    virtual ~LogSink() = default;

    /// Receives one message.
    ///
    /// \param severity How severe the message is.
    /// \param message The message, a line of text with no line break at its end.
    virtual void write(LogSeverity severity, std::string_view message) = 0;

  protected:
    /// Creates the sink.
    LogSink() = default;

    /// Copies a sink, for a derived class's copy constructor.
    LogSink(const LogSink&) = default;

    /// Moves a sink, for a derived class's move constructor.
    LogSink(LogSink&&) = default;

    /// Copies a sink, for a derived class's copy assignment.
    LogSink& operator=(const LogSink&) = default;

    /// Moves a sink, for a derived class's move assignment.
    LogSink& operator=(LogSink&&) = default;
};

/// Sends aplomb's log messages to `sink`, or back to the default sink with `nullptr`.
///
/// The default sink writes each message to stderr as a line that begins with its severity, such as
/// `warning: ...`. aplomb doesn't own `sink`, which must outlive its use. Set the sink once at
/// start-up, before any other thread is running.
///
/// \param sink The sink to write to from now on, or `nullptr` for the default.
void set_log_sink(LogSink* sink) noexcept;

/// Writes a message to the current log sink. It may be called from any thread.
///
/// \param severity How severe the message is.
/// \param message The message, a line of text with no line break at its end.
void log(LogSeverity severity, std::string_view message);

} // namespace aplomb
