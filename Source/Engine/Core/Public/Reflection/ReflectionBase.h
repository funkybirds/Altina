#pragma once
#include "Base/CoreAPI.h"
#include "Types/Traits.h"
#include "Types/Meta.h"
namespace AltinaEngine::Core::Reflection
{
    using namespace TypeMeta;
    constexpr bool kEnableRuntimeSanityCheck = true;

    enum class EReflectionErrorCode : u8
    {
        Success                  = 0,
        TypeNotCopyConstructible = 1,
        TypeNotDestructible      = 2,
        CorruptedAnyCast         = 3,
    };

    [[noreturn]] AE_CORE_API void ReflectionAbort(
        EReflectionErrorCode errorCode, const FMetaTypeInfo& tpInfo, void* objInfo);

    inline bool ReflectionAssert(
        bool condition, EReflectionErrorCode errorCode, const FMetaTypeInfo& tpInfo, void* objInfo)
    {
        if constexpr (kEnableRuntimeSanityCheck)
        {
            if (!condition) [[unlikely]]
            {
                ReflectionAbort(errorCode, tpInfo, objInfo);
            }
        }
        return condition;
    }

} // namespace AltinaEngine::Core::Reflection