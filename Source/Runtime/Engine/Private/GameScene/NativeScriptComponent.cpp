#include "Engine/GameScene/NativeScriptComponent.h"

namespace AltinaEngine::GameScene {
    FNativeScriptComponent::FNativeScriptComponent() = default;

    FNativeScriptComponent::FNativeScriptComponent(const FNativeScriptComponent& rhs)
        : FScriptComponentBase(rhs) {}

    auto FNativeScriptComponent::operator=(const FNativeScriptComponent& rhs)
        -> FNativeScriptComponent& {
        if (this != &rhs) {
            FScriptComponentBase::operator=(rhs);
        }
        return *this;
    }

    FNativeScriptComponent::~FNativeScriptComponent() = default;
} // namespace AltinaEngine::GameScene
