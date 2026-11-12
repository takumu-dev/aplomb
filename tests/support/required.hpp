#pragma once

#include <catch2/catch_test_macros.hpp>

/// The value of an optional, which the test requires it to have. Where it has none, the test
/// fails there.
template <typename Optional> auto& required(Optional& optional) {
    REQUIRE(optional.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access): the REQUIRE above checks it
    return *optional;
}
