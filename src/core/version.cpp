#include <aplomb/core/version.hpp>

#include <Jolt/Jolt.h>

#include <array>
#include <cstddef>
#include <string_view>

namespace aplomb {

namespace {

// The characters of a version, such as `5.6.0`, and how many of them there are.
struct VersionText {
    std::array<char, 32> characters{};
    std::size_t size = 0;
};

// Jolt's version, spelt out once when aplomb is compiled, so that it lasts as long as the program.
constexpr VersionText jolt_version_text = [] {
    VersionText text;
    const auto append = [&text](char character) { text.characters.at(text.size++) = character; };
    const auto append_number = [&append](int number) {
        constexpr int base = 10;
        std::array<char, 16> digits{};
        std::size_t count = 0;
        // Zero has a digit too.
        for (int rest = number; count == 0 || rest > 0; rest /= base) {
            digits.at(count++) = static_cast<char>('0' + (rest % base));
        }
        while (count > 0) {
            append(digits.at(--count));
        }
    };
    append_number(JPH_VERSION_MAJOR);
    append('.');
    append_number(JPH_VERSION_MINOR);
    append('.');
    append_number(JPH_VERSION_PATCH);
    return text;
}();

} // namespace

std::string_view version() noexcept { return APLOMB_VERSION; }

std::string_view jolt_version() noexcept {
    return {jolt_version_text.characters.data(), jolt_version_text.size};
}

} // namespace aplomb
