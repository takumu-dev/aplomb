// A throw in code built with the library's settings, which a build without exceptions must reject.
#include <stdexcept>

namespace aplomb {

// Declared before its definition, as a function of the library is in its header, so that no check
// of a function without a declaration fails first.
void throw_stray_exception();

void throw_stray_exception() { throw std::runtime_error{"a stray exception"}; }

} // namespace aplomb
