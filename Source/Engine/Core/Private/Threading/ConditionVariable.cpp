#include "../../Public/Threading/ConditionVariable.h"
#include "../../Public/Threading/Mutex.h"
#include "../../Public/Platform/Generic/GenericPlatformDecl.h"

namespace AltinaEngine::Core::Threading
{

    using namespace AltinaEngine::Core::Platform::Generic;

    FConditionVariable::FConditionVariable() noexcept : mImpl(PlatformCreateConditionVariable()) {}

    FConditionVariable::~FConditionVariable() noexcept
    {
        if (mImpl)
        {
            PlatformDeleteConditionVariable(mImpl);
            mImpl = nullptr;
        }
    }

    void FConditionVariable::NotifyOne() noexcept { PlatformWakeConditionVariable(mImpl); }

    void FConditionVariable::NotifyAll() noexcept { PlatformWakeAllConditionVariable(mImpl); }

    auto FConditionVariable::Wait(FMutex& Mutex, unsigned long Milliseconds) noexcept -> bool
    {
        return PlatformSleepConditionVariableCS(mImpl, Mutex.GetNative(), Milliseconds) != 0;
    }

} // namespace AltinaEngine::Core::Threading
