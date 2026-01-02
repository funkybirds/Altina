#pragma once
#include "Base/AltinaBase.h"
#include "BasicTraits.h"

namespace AltinaEngine::Detail::CompilerTraits
{
#if (AE_COMPILER_MSVC || AE_COMPILER_GCC || AE_COMPILER_CLANG)
    template <typename T> using TTypeIsUnionImpl = TBoolConstant<__is_union(T)>;
    template <typename T, typename... TArgs>
    using TTypeIsTriviallyConstructibleImpl = TBoolConstant<__is_trivially_constructible(T, TArgs...)>;
#else
    #error "Unsupported compiler"
#endif

} // namespace AltinaEngine::Detail::CompilerTraits
