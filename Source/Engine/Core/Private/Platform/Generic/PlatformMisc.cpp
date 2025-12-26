#include "Platform/Generic/GenericPlatformDecl.h"

#include <cstdlib>
#include <cstring>
#include <exception>

namespace AltinaEngine::Core::Platform::Generic
{

    extern "C"
    {

        AE_CORE_API void  PlatformAbort() { std::abort(); }

        AE_CORE_API void  PlatformTerminate() { std::terminate(); }

        AE_CORE_API void* Memset(void* Dest, int Value, usize Count) { return std::memset(Dest, Value, Count); }

        AE_CORE_API void* Memcpy(void* Dest, const void* Src, usize Count) { return std::memcpy(Dest, Src, Count); }

    } // extern "C"

} // namespace AltinaEngine::Core::Platform::Generic
