#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/ScriptComponentBase.h"
#include "Reflection/ReflectionAnnotations.h"

namespace AltinaEngine::GameScene {
    /**
     * @brief Base class for native (C++) gameplay scripts.
     *
     * Native scripts are regular components: ticking is virtual dispatch with no reflection
     * invocation overhead. This base is abstract to avoid accidental instantiation/serialization.
     */
    class ACLASS(Abstract) AE_ENGINE_API FNativeScriptComponent : public FScriptComponentBase {
    public:
        ~FNativeScriptComponent() override;

        FNativeScriptComponent(const FNativeScriptComponent& rhs);
        auto operator=(const FNativeScriptComponent& rhs) -> FNativeScriptComponent&;

        void Tick(float dt) override = 0;

    protected:
        FNativeScriptComponent();
    };
} // namespace AltinaEngine::GameScene
