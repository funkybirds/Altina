#include "Reflection/ReflectionBase.h"
#include "Logging/Log.h"
namespace AltinaEngine::Core::Reflection
{
    [[noreturn]] AE_CORE_API void ReflectionAbort(EReflectionErrorCode errorCode, const FReflectionDumpData& dumpData)
    {
        switch (errorCode)
        {
            case EReflectionErrorCode::TypeNotCopyConstructible:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Type is not copy constructible"));
                break;
            case EReflectionErrorCode::TypeNotDestructible:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Type is not destructible"));
                break;
            case EReflectionErrorCode::CorruptedAnyCast:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Corrupted Any cast operation"));
                break;
            case EReflectionErrorCode::MismatchedArgumentNumber:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Mismatched argument number"));
                break;
            case EReflectionErrorCode::TypeHashConflict:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Type hash conflict detected"));
                break;
            case EReflectionErrorCode::TypeUnregistered:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Type is not registered"));
                break;
            case EReflectionErrorCode::ObjectAndTypeMismatch:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Object and type metadata mismatch"));
                break;
            case EReflectionErrorCode::PropertyUnregistered:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Property is not registered"));
                break;
            case EReflectionErrorCode::DereferenceNullptr:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Dereferencing null pointer"));
                break;
            default:
                LogErrorCat(TEXT("Core.Reflection"), TEXT("Unknown error"));
        }

        // Silence unused parameter warnings (build uses /WX)
        (void)errorCode;
        (void)dumpData;
        std::abort();
    }
} // namespace AltinaEngine::Core::Reflection