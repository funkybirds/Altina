#include "Engine/GameScene/Component.h"

#include "Engine/GameScene/World.h"

namespace AltinaEngine::GameScene {
    void FComponent::SetEnabled(bool enabled) noexcept {
        if (mEnabled == enabled) {
            return;
        }

        mEnabled = enabled;
        if (mEnabled) {
            OnEnable();
        } else {
            OnDisable();
        }

        if (mWorld != nullptr) {
            mWorld->OnComponentEnabledChanged(mId, mOwner, mEnabled);
        }
    }
} // namespace AltinaEngine::GameScene
