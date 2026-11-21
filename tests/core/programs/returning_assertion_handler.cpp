// Fails a check under an assertion handler that wrongly returns, after which aplomb must abort.
#include <aplomb/core/assertion.hpp>

#include <cstdlib>

namespace {

void return_anyway(const aplomb::AssertionFailure& /*failure*/) {}

} // namespace

int main() {
#ifdef _MSC_VER
    // Abort without the dialog of MSVC's debug runtime, which would hold the test up.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    aplomb::set_assertion_handler(return_anyway);
    const int sides = 3;
    APLOMB_ASSERT(sides == 4, "a square has four sides");
    return EXIT_SUCCESS;
}
