#pragma once

#include "RenderCoreAPI.h"

#include "Math/LinAlg/SpatialTransform.h"
#include "Math/Quaternion.h"
#include "Math/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::RenderCore::View {
    namespace Math   = Core::Math;
    namespace LinAlg = Core::Math::LinAlg;

    enum class ECameraProjectionType : u8 {
        Perspective = 0,
        Orthographic
    };

    struct AE_RENDER_CORE_API FCameraData {
        ECameraProjectionType     mProjectionType = ECameraProjectionType::Perspective;

        f32                       mVerticalFovRadians = 1.04719755f; // ~60 deg
        f32                       mNearPlane          = 0.1f;
        f32                       mFarPlane           = 10000.0f;

        f32                       mOrthoWidth  = 512.0f;
        f32                       mOrthoHeight = 512.0f;

        LinAlg::FSpatialTransform mTransform = LinAlg::FSpatialTransform::Identity();

        bool                      mCameraCut = false;

        [[nodiscard]] auto        GetPosition() const noexcept -> const Math::FVector3f& {
            return mTransform.Translation;
        }

        [[nodiscard]] auto GetRotation() const noexcept -> const Math::FQuaternion& {
            return mTransform.Rotation;
        }

        [[nodiscard]] auto GetScale() const noexcept -> const Math::FVector3f& {
            return mTransform.Scale;
        }
    };

} // namespace AltinaEngine::RenderCore::View
