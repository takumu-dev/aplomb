#include "precondition_test.hpp"
#include <aplomb/files/metrics_file.hpp>

void break_precondition() {
    // 0xff starts no UTF-8 sequence.
    [[maybe_unused]] const auto text =
        aplomb::write_metrics_file({.scenario = "drop\xff", .step_rate = 60.0F});
}
