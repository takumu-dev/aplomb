// Reads past the end of a heap allocation, which AddressSanitizer must stop.
#include <cstddef>
#include <vector>

int main() {
    const std::vector<int> values(4);
    // Volatile, so that the compiler can't see the read is out of bounds. The read goes through a
    // pointer, since the standard library's own checks would stop it in a container.
    const volatile std::size_t past_end = values.size();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): the overflow is the point.
    return *(values.data() + past_end);
}
