#include "../../Public/Threading/Event.h"
#include "../../Public/Platform/Generic/GenericPlatformDecl.h"

namespace AltinaEngine::Core::Threading {

using namespace AltinaEngine::Core::Platform::Generic;

FEvent::FEvent(bool bInitiallySignaled, EEventResetMode ResetMode) noexcept {
    Impl = PlatformCreateEvent(ResetMode == EEventResetMode::Manual ? 1 : 0,
                               bInitiallySignaled ? 1 : 0);
}

FEvent::~FEvent() noexcept {
    if (Impl) {
        PlatformCloseEvent(Impl);
        Impl = nullptr;
    }
}

void FEvent::Set() noexcept {
    PlatformSetEvent(Impl);
}

void FEvent::Reset() noexcept {
    PlatformResetEvent(Impl);
}

bool FEvent::Wait(unsigned long Milliseconds) noexcept {
    return PlatformWaitForEvent(Impl, Milliseconds) != 0;
}

} // namespace
