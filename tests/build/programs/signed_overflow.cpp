// Overflows a signed integer, which UndefinedBehaviorSanitizer must stop.
#include <limits>

int main(int argc, char** /*argv*/) {
    int value = std::numeric_limits<int>::max();
    // argc is at least 1, but the compiler can't know it, so the addition happens at run time.
    value += argc;
    return value < 0 ? 0 : 1;
}
