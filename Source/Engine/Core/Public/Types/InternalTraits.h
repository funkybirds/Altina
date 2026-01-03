#pragma once
#include "Base/AltinaBase.h"
#include "BasicTraits.h"

namespace AltinaEngine::Detail::CompilerTraits {
#if (AE_COMPILER_MSVC || AE_COMPILER_GCC || AE_COMPILER_CLANG)
    template <typename T> using TTypeIsUnionImpl = TBoolConstant<__is_union(T)>;
    template <typename T> using TTypeIsEnumImpl  = TBoolConstant<__is_enum(T)>;
    template <typename T, typename... TArgs>
    using TTypeIsTriviallyConstructibleImpl =
        TBoolConstant<__is_trivially_constructible(T, TArgs...)>;
    template <typename T> using TUnderlyingTypeImpl = __underlying_type(T);
#else
    #error "Unsupported compiler"
#endif

} // namespace AltinaEngine::Detail::CompilerTraits
