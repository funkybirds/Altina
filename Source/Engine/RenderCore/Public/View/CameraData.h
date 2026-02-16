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
        ECameraProjectionType     ProjectionType = ECameraProjectionType::Perspective;

        f32                       VerticalFovRadians = 1.04719755f; // ~60 deg
        f32                       NearPlane          = 0.1f;
        f32                       FarPlane           = 10000.0f;

        f32                       OrthoWidth  = 512.0f;
        f32                       OrthoHeight = 512.0f;

        LinAlg::FSpatialTransform Transform = LinAlg::FSpatialTransform::Identity();

        bool                      bCameraCut = false;

        [[nodiscard]] auto        GetPosition() const noexcept -> const Math::FVector3f& {
            return Transform.Translation;
        }

        [[nodiscard]] auto GetRotation() const noexcept -> const Math::FQuaternion& {
            return Transform.Rotation;
        }

        [[nodiscard]] auto GetScale() const noexcept -> const Math::FVector3f& {
            return Transform.Scale;
        }
    };

} // namespace AltinaEngine::RenderCore::View
