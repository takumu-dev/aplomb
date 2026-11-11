/// \file
/// Checks of preconditions and invariants, and the handler that a failed check calls.
///
/// A broken precondition is a bug in the caller, not an error to return. `APLOMB_ASSERT` checks
/// the cheap preconditions of the public interface in every build, release included;
/// `APLOMB_DEBUG_ASSERT` checks internal invariants and expensive conditions outside release
/// builds. A failed check is logged as an error and then passed to the assertion handler, which
/// aborts unless a host has replaced it.

#pragma once

#include <source_location>
#include <string_view>

/// aplomb, an active ragdoll library on Jolt Physics.
namespace aplomb {

/// A failed check, as the assertion handler receives it.
struct AssertionFailure {
    /// The condition that was false, as it is written in the check.
    std::string_view expression;

    /// Why the condition must hold.
    std::string_view message;

    /// Where the check is written.
    std::source_location location;
};

/// A function that a failed check calls. It must not return, and if it does, aplomb aborts.
///
/// It may be called from any thread, and from several at once.
using AssertionHandler = void (*)(const AssertionFailure& failure);

/// Replaces the function that a failed check calls, or restores the default with `nullptr`.
///
/// The default handler calls `std::abort()`. A host might replace it to send the failure to its
/// crash reporter. Set the handler once at start-up, before any other thread is running.
///
/// \param handler The handler to call from now on, or `nullptr` for the default.
void set_assertion_handler(AssertionHandler handler) noexcept;

namespace detail {

/// Logs `failure` as an error, then calls the assertion handler and aborts if it returns.
[[noreturn]] void fail_assertion(const AssertionFailure& failure);

} // namespace detail

} // namespace aplomb

// NOLINTBEGIN(cppcoreguidelines-macro-usage): a check reports its condition as written and the
// place it is written, which only a macro can see.

/// Checks a cheap precondition of the public interface, in every build.
///
/// The condition is evaluated once. If it is false, the failure is logged as an error and the
/// assertion handler is called, which doesn't return.
///
/// \param condition The condition that must hold.
/// \param message Why it must hold, as a string literal.
#define APLOMB_ASSERT(condition, message)                                                          \
    (static_cast<bool>(condition)                                                                  \
         ? static_cast<void>(0)                                                                    \
         : ::aplomb::detail::fail_assertion(::aplomb::AssertionFailure{                            \
               #condition, (message), ::std::source_location::current()}))

/// Checks an internal invariant or an expensive condition, outside release builds.
///
/// Where `NDEBUG` is defined, as in release builds, the condition is never evaluated, though it
/// must still compile. Elsewhere this is `APLOMB_ASSERT`.
///
/// \param condition The condition that must hold.
/// \param message Why it must hold, as a string literal.
#ifdef NDEBUG
#define APLOMB_DEBUG_ASSERT(condition, message)                                                    \
    static_cast<void>(sizeof(static_cast<bool>(condition)))
#else
#define APLOMB_DEBUG_ASSERT(condition, message) APLOMB_ASSERT(condition, message)
#endif

// NOLINTEND(cppcoreguidelines-macro-usage)
