#pragma once
#include "Base/CoreAPI.h"
#include "Types/Meta.h"
namespace AltinaEngine::Core::Reflection {
    using namespace TypeMeta;
    constexpr bool kEnableRuntimeSanityCheck = true;

    enum class EReflectionErrorCode : u8 {
        Success                  = 0,
        TypeNotCopyConstructible = 1,
        TypeNotDestructible      = 2,
        CorruptedAnyCast         = 3,
        MismatchedArgumentNumber = 4,

        TypeHashConflict      = 5,
        TypeUnregistered      = 6,
        ObjectAndTypeMismatch = 7,
        PropertyUnregistered  = 8,

        DereferenceNullptr = 9
    };

    struct FReflectionDumpData {
        void*            mObjInfo;
        FMetaMethodInfo* mMethodInfo;
    };

    [[noreturn]] AE_CORE_API void ReflectionAbort(
        EReflectionErrorCode errorCode, const FReflectionDumpData& dumpData);

    constexpr auto ReflectionAssert(bool condition, EReflectionErrorCode errorCode,
        const FReflectionDumpData& dumpData) -> bool {
        if constexpr (kEnableRuntimeSanityCheck) {
            if (!condition) [[unlikely]] {
                ReflectionAbort(errorCode, dumpData);
            }
        }
        return condition;
    }

} // namespace AltinaEngine::Core::Reflection