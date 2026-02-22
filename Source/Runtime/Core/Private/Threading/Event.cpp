#include "../../Public/Threading/Event.h"
#include "../../Public/Platform/Generic/GenericPlatformDecl.h"

namespace AltinaEngine::Core::Threading {

    using namespace AltinaEngine::Core::Platform::Generic;

    FEvent::FEvent(bool bInitiallySignaled, EEventResetMode ResetMode) noexcept
        : mImpl(PlatformCreateEvent(
              ResetMode == EEventResetMode::Manual ? 1 : 0, bInitiallySignaled ? 1 : 0)) {}

    FEvent::~FEvent() noexcept {
        if (mImpl) {
            PlatformCloseEvent(mImpl);
            mImpl = nullptr;
        }
    }

    void FEvent::Set() noexcept { PlatformSetEvent(mImpl); }

    void FEvent::Reset() noexcept { PlatformResetEvent(mImpl); }

    auto FEvent::Wait(unsigned long Milliseconds) noexcept -> bool {
        return PlatformWaitForEvent(mImpl, Milliseconds) != 0;
    }

} // namespace AltinaEngine::Core::Threading
