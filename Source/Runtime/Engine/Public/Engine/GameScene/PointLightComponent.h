#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Component.h"
#include "Math/Vector.h"
#include "Reflection/ReflectionAnnotations.h"
#include "Reflection/ReflectionFwd.h"

namespace AltinaEngine::GameScene {
    namespace Math = Core::Math;

    /**
     * @brief Simple point light component.
     *
     * Position is derived from the owner's world translation.
     */
    class ACLASS() AE_ENGINE_API FPointLightComponent : public FComponent {
    private:
        template <auto Member>
        friend struct AltinaEngine::Core::Reflection::Detail::TAutoMemberAccessor;

    public:
        APROPERTY()
        Math::FVector3f mColor = Math::FVector3f(1.0f, 1.0f, 1.0f);

        APROPERTY()
        f32 mIntensity = 5.0f;

        APROPERTY()
        f32 mRange = 10.0f;
    };
} // namespace AltinaEngine::GameScene
