#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Component.h"
#include "Math/LinAlg/ProjectionMatrix.h"
#include "Reflection/ReflectionAnnotations.h"
#include "Reflection/ReflectionFwd.h"

namespace AltinaEngine::GameScene {
    namespace Math   = Core::Math;
    namespace LinAlg = Core::Math::LinAlg;

    /**
     * @brief Simple camera component storing projection settings.
     */
    class ACLASS() AE_ENGINE_API FCameraComponent : public FComponent {
    public:
        [[nodiscard]] auto GetFovYRadians() const noexcept -> f32 { return mFovYRadians; }
        void               SetFovYRadians(f32 fovYRadians) noexcept { mFovYRadians = fovYRadians; }

        [[nodiscard]] auto GetNearPlane() const noexcept -> f32 { return mNearPlane; }
        void               SetNearPlane(f32 nearPlane) noexcept { mNearPlane = nearPlane; }

        [[nodiscard]] auto GetFarPlane() const noexcept -> f32 { return mFarPlane; }
        void               SetFarPlane(f32 farPlane) noexcept { mFarPlane = farPlane; }

        [[nodiscard]] auto BuildProjection(f32 viewWidth, f32 viewHeight) const noexcept
            -> Math::FMatrix4x4f {
            return LinAlg::FProjectionMatrixf(
                mFovYRadians, viewWidth, viewHeight, mNearPlane, mFarPlane);
        }

    private:
        template <auto Member>
        friend struct AltinaEngine::Core::Reflection::Detail::TAutoMemberAccessor;

    public:
        APROPERTY()
        f32 mFovYRadians = Math::kPiF / 3.0f;

        APROPERTY()
        f32 mNearPlane = 0.1f;

        APROPERTY()
        f32 mFarPlane = 1000.0f;
    };
} // namespace AltinaEngine::GameScene
