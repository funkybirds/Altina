#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Component.h"
#include "Math/Vector.h"
#include "Reflection/ReflectionAnnotations.h"
#include "Reflection/ReflectionFwd.h"

namespace AltinaEngine::GameScene {
    namespace Math = Core::Math;

    /**
     * @brief Simple directional light component.
     *
     * Direction is derived from the owner's world rotation. The owner's +Z axis is treated as the
     * light propagation direction (from light towards the scene).
     */
    class ACLASS() AE_ENGINE_API FDirectionalLightComponent : public FComponent {
    private:
        template <auto Member>
        friend struct AltinaEngine::Core::Reflection::Detail::TAutoMemberAccessor;

    public:
        APROPERTY()
        Math::FVector3f mColor = Math::FVector3f(1.0f, 1.0f, 1.0f);

        APROPERTY()
        f32 mIntensity = 1.0f;

        APROPERTY()
        bool mCastShadows = true;
    };
} // namespace AltinaEngine::GameScene
