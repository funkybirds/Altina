#include "Reflection/ReflectionBase.h"
#include "Logging/Log.h"
namespace AltinaEngine::Core::Reflection
{
    [[noreturn]] AE_CORE_API void ReflectionAbort(
        EReflectionErrorCode errorCode, const FMetaTypeInfo& tpInfo, void* objInfo)
    {
        // Silence unused parameter warnings (build uses /WX)
        (void)errorCode;
        (void)tpInfo;
        (void)objInfo;
        std::abort();
    }
} // namespace AltinaEngine::Core::Reflection