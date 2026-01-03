#pragma once
#include <cstdlib>
namespace AltinaEngine::Core::Utility::CompilerHint {
    [[noreturn]] inline void Unreachable() {
#if defined(_MSC_VER) && !defined(__clang__) // MSVC
        __assume(false);
#else // GCC, Clang
        __builtin_unreachable();
#endif
    }
} // namespace AltinaEngine::Core::Utility::CompilerHint