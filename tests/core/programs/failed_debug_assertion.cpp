// Fails a debug assertion. Outside release builds it must reach the handler; where NDEBUG is
// defined, as in release builds, it must do nothing. Either way the program ends successfully only
// if the build behaved as it should.
#include <aplomb/core/assertion.hpp>

#include <cstdlib>

namespace {

#ifdef NDEBUG
constexpr bool fires = false;
#else
constexpr bool fires = true;
#endif

[[noreturn]] void exit_fired(const aplomb::AssertionFailure& /*failure*/) {
    std::_Exit(fires ? EXIT_SUCCESS : EXIT_FAILURE);
}

} // namespace

int main() {
    aplomb::set_assertion_handler(exit_fired);
    const int sides = 3;
    APLOMB_DEBUG_ASSERT(sides == 4, "a square has four sides");
    return fires ? EXIT_FAILURE : EXIT_SUCCESS;
}
