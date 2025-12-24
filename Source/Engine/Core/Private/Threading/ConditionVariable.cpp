#include "../../Public/Threading/ConditionVariable.h"
#include "../../Public/Threading/Mutex.h"
#include "../../Public/Platform/Generic/GenericPlatformDecl.h"

namespace AltinaEngine::Core::Threading {

using namespace AltinaEngine::Core::Platform::Generic;

FConditionVariable::FConditionVariable() noexcept {
    Impl = PlatformCreateConditionVariable();
}

FConditionVariable::~FConditionVariable() noexcept {
    if (Impl) {
        PlatformDeleteConditionVariable(Impl);
        Impl = nullptr;
    }
}

void FConditionVariable::NotifyOne() noexcept {
    PlatformWakeConditionVariable(Impl);
}

void FConditionVariable::NotifyAll() noexcept {
    PlatformWakeAllConditionVariable(Impl);
}

bool FConditionVariable::Wait(FMutex& Mutex, unsigned long Milliseconds) noexcept {
    return PlatformSleepConditionVariableCS(Impl, Mutex.GetNative(), Milliseconds) != 0;
}

} // namespace
