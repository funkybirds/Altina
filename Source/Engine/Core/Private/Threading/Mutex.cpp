#include "../../Public/Threading/Mutex.h"
#include "../../Public/Platform/Generic/GenericPlatformDecl.h"

namespace AltinaEngine::Core::Threading
{

    using namespace AltinaEngine::Core::Platform::Generic;

    FMutex::FMutex() noexcept { mImpl = PlatformCreateCriticalSection(); }

    FMutex::~FMutex() noexcept
    {
        if (mImpl)
        {
            PlatformDeleteCriticalSection(mImpl);
            mImpl = nullptr;
        }
    }

    void FMutex::Lock() noexcept { PlatformEnterCriticalSection(mImpl); }

    auto FMutex::TryLock() noexcept -> bool { return PlatformTryEnterCriticalSection(mImpl) != 0; }

    void FMutex::Unlock() noexcept { PlatformLeaveCriticalSection(mImpl); }

    auto FMutex::GetNative() const noexcept -> void* { return mImpl; }

    FScopedLock::FScopedLock(FMutex& InMutex) noexcept : mMutex(InMutex) { mMutex.Lock(); }

    FScopedLock::~FScopedLock() noexcept { mMutex.Unlock(); }

} // namespace AltinaEngine::Core::Threading
