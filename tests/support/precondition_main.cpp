// The main program of a precondition test, run with the message of the one check it must fail. A
// precondition that fires calls the assertion handler, which this replaces with one that fails the
// process when that check is the one that failed. CTest expects the test to fail; if no check
// fails, or another one does, the program succeeds and the test fails.
//
// The default handler would abort, but CTest counts an abort as a crash even in a test expected to
// fail.
#include "precondition_test.hpp"
#include <aplomb/core/assertion.hpp>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <print>
#include <span>
#include <string_view>

namespace {

// A handler is a plain function and can't capture, so what it compares lives here.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): the handler reads it.
std::string_view expected_message;

[[noreturn]] void exit_failure_if_expected(const aplomb::AssertionFailure& failure) {
    if (failure.message == expected_message) {
        std::_Exit(EXIT_FAILURE);
    }
    std::println(stderr, "another check failed than \"{}\"", expected_message);
    std::_Exit(EXIT_SUCCESS);
}

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): nothing is caught, so a failure ends the program.
int main(int argc, char** argv) {
    const std::span arguments(argv, static_cast<std::size_t>(argc));
    if (arguments.size() != 2) {
        std::println(stderr, "usage: <precondition test> <message of the check that must fail>");
        return EXIT_SUCCESS;
    }
    expected_message = arguments.back();
    aplomb::set_assertion_handler(exit_failure_if_expected);
    break_precondition();
    return EXIT_SUCCESS;
}
