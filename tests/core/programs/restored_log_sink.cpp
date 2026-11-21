// Replaces the log sink and then restores the default, which the test reads from stderr.
#include <aplomb/core/log_sink.hpp>

#include <string_view>

namespace {

class SilentSink final : public aplomb::LogSink {
  public:
    void write(aplomb::LogSeverity /*severity*/, std::string_view /*message*/) override {}
};

} // namespace

int main() {
    SilentSink sink;
    aplomb::set_log_sink(&sink);
    aplomb::log(aplomb::LogSeverity::info, "to the replaced sink");
    aplomb::set_log_sink(nullptr);
    aplomb::log(aplomb::LogSeverity::info, "to the default sink");
}
