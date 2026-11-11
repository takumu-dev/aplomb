#include <aplomb/core/assertion.hpp>

#include <catch2/catch_test_macros.hpp>

namespace {

// Counts each time it is called, and holds.
bool holds_counting(int& evaluations) {
    ++evaluations;
    return true;
}

} // namespace

TEST_CASE("an assertion evaluates its condition once in every build", "[core]") {
    int evaluations = 0;

    APLOMB_ASSERT(holds_counting(evaluations), "the condition holds");

    CHECK(evaluations == 1);
}

TEST_CASE("a debug assertion evaluates its condition only outside release builds", "[core]") {
    int evaluations = 0;

    APLOMB_DEBUG_ASSERT(holds_counting(evaluations), "the condition holds");

#ifdef NDEBUG
    CHECK(evaluations == 0);
#else
    CHECK(evaluations == 1);
#endif
}
