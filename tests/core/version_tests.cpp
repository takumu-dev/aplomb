#include <aplomb/core/version.hpp>

#include <Jolt/Jolt.h>

#include <catch2/catch_test_macros.hpp>

#include <format>

TEST_CASE("aplomb's version is the project's", "[core]") {
    CHECK(aplomb::version() == APLOMB_PROJECT_VERSION);
}

TEST_CASE("Jolt's version is the one aplomb is built with", "[core]") {
    CHECK(aplomb::jolt_version() ==
          std::format("{}.{}.{}", JPH_VERSION_MAJOR, JPH_VERSION_MINOR, JPH_VERSION_PATCH));
}
