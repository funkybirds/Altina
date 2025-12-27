#pragma once

#include "Types/Aliases.h"
#include "Math/LinAlg/Common.h"

namespace AltinaEngine::Core::Math::Physics
{
    // Planar primitives
    [[nodiscard]] AE_FORCEINLINE f32 DiskMomentOfInertiaWrtCenter2D(f32 radius, f32 mass)
    {
        return 0.5f * mass * radius * radius;
    }

    [[nodiscard]] AE_FORCEINLINE f32 RectMomentOfInertiaWrtCenter2D(f32 width, f32 height, f32 mass)
    {
        return (1.0f / 12.0f) * mass * (width * width + height * height);
    }

    // Solid primitives
    [[nodiscard]] AE_FORCEINLINE FMatrix3x3f SphereMomentOfInertiaWrtCenter3D(f32 radius, f32 mass)
    {
        // I = 2/5 * m * r^2
        f32         inertia       = (2.0f / 5.0f) * mass * radius * radius;
        FMatrix3x3f inertiaTensor = LinAlg::ZeroMatrix<f32, 3, 3>();
        inertiaTensor[0][0]       = inertia;
        inertiaTensor[1][1]       = inertia;
        inertiaTensor[2][2]       = inertia;
        return inertiaTensor;
    }

    [[nodiscard]] AE_FORCEINLINE FMatrix3x3f CuboidMomentOfInertiaWrtCenter3D(
        f32 width, f32 height, f32 depth, f32 mass)
    {
        // Ixx = 1/12 * m * (h^2 + d^2)
        // Iyy = 1/12 * m * (w^2 + d^2)
        // Izz = 1/12 * m * (w^2 + h^2)
        FMatrix3x3f inertiaTensor = LinAlg::ZeroMatrix<f32, 3, 3>();
        inertiaTensor[0][0]       = (1.0f / 12.0f) * mass * (height * height + depth * depth);
        inertiaTensor[1][1]       = (1.0f / 12.0f) * mass * (width * width + depth * depth);
        inertiaTensor[2][2]       = (1.0f / 12.0f) * mass * (width * width + height * height);
        return inertiaTensor;
    }

} // namespace AltinaEngine::Core::Math::Physics
