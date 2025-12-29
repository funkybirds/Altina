#include "Reflection/ReflectionBase.h"
#include "Logging/Log.h"
namespace AltinaEngine::Core::Reflection
{
    [[noreturn]] AE_CORE_API void ReflectionAbort(EReflectionErrorCode errorCode, const FReflectionDumpData& dumpData)
    {
        // Silence unused parameter warnings (build uses /WX)
        (void)errorCode;
        (void)dumpData;
        std::abort();
    }
} // namespace AltinaEngine::Core::Reflection