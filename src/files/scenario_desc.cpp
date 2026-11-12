#include <aplomb/files/scenario_desc.hpp>

#include <string_view>

namespace aplomb {

std::string_view metric_name(Metric metric) noexcept {
    switch (metric) {
    case Metric::max_speed_after_3s:
        return "max_speed_after_3s";
    case Metric::nan_count:
        return "nan_count";
    case Metric::state_hash_final:
        return "state_hash_final";
    }
    // Only a metric cast from outside the enumeration gets here.
    return "unknown";
}

std::string_view comparison_name(Comparison comparison) noexcept {
    switch (comparison) {
    case Comparison::less:
        return "lt";
    case Comparison::less_or_equal:
        return "le";
    case Comparison::equal:
        return "eq";
    case Comparison::greater_or_equal:
        return "ge";
    case Comparison::greater:
        return "gt";
    }
    // Only a comparison cast from outside the enumeration gets here.
    return "unknown";
}

bool compare(Comparison comparison, double measured, double value) noexcept {
    switch (comparison) {
    case Comparison::less:
        return measured < value;
    case Comparison::less_or_equal:
        return measured <= value;
    case Comparison::equal:
        return measured == value;
    case Comparison::greater_or_equal:
        return measured >= value;
    case Comparison::greater:
        return measured > value;
    }
    return false;
}

} // namespace aplomb
