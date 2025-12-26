#include "../../Public/Threading/Mutex.h"
#include "../../Public/Platform/Generic/GenericPlatformDecl.h"

namespace AltinaEngine::Core::Threading
{

    using namespace AltinaEngine::Core::Platform::Generic;

    FMutex::FMutex() noexcept { Impl = PlatformCreateCriticalSection(); }

    FMutex::~FMutex() noexcept
    {
        if (Impl)
        {
            PlatformDeleteCriticalSection(Impl);
            Impl = nullptr;
        }
    }

    void  FMutex::Lock() noexcept { PlatformEnterCriticalSection(Impl); }

    bool  FMutex::TryLock() noexcept { return PlatformTryEnterCriticalSection(Impl) != 0; }

    void  FMutex::Unlock() noexcept { PlatformLeaveCriticalSection(Impl); }

    void* FMutex::GetNative() const noexcept { return Impl; }

    FScopedLock::FScopedLock(FMutex& InMutex) noexcept : Mutex(InMutex) { Mutex.Lock(); }

    FScopedLock::~FScopedLock() noexcept { Mutex.Unlock(); }

} // namespace AltinaEngine::Core::Threading
