#pragma once

#include "Math/Common.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Math/LinAlg/Common.h"
#include <climits>

namespace AltinaEngine::Core::Math::LinAlg {
    [[nodiscard]] auto ComputeNormalMatrix(const FMatrix4x4f& world) noexcept -> FMatrix4x4f {
        using Core::Math::FMatrix3x3f;

        // Normal matrix = transpose(inverse(upper3x3(World))).
        FMatrix3x3f upper{};
        for (u32 r = 0U; r < 3U; ++r) {
            for (u32 c = 0U; c < 3U; ++c) {
                upper(r, c) = world(r, c);
            }
        }

        const f32         det         = Core::Math::LinAlg::Determinant(upper);
        const bool        bInvertible = std::abs(det) > 1e-8f;

        const FMatrix3x3f normal3 = bInvertible
            ? Core::Math::Transpose(Core::Math::LinAlg::Inverse(upper))
            : Core::Math::LinAlg::Identity<f32, 3U>();

        FMatrix4x4f       normal4 = Core::Math::LinAlg::Identity<f32, 4U>();
        for (u32 r = 0U; r < 3U; ++r) {
            for (u32 c = 0U; c < 3U; ++c) {
                normal4(r, c) = normal3(r, c);
            }
        }
        return normal4;
    }

    [[nodiscard]] auto TransformAabbToWorld(const FMatrix4x4f& world, const FVector3f& bboxMax,
        const FVector3f& bboxMin, FVector3f& outMinWS, FVector3f& outMaxWS) -> bool {

        const FVector3f& bmin = bboxMin;
        const FVector3f& bmax = bboxMax;

        FVector3f        minWS(std::numeric_limits<f32>::max());
        FVector3f        maxWS(-std::numeric_limits<f32>::max());

        const f32        xs[2] = { bmin[0], bmax[0] };
        const f32        ys[2] = { bmin[1], bmax[1] };
        const f32        zs[2] = { bmin[2], bmax[2] };

        for (u32 xi = 0U; xi < 2U; ++xi) {
            for (u32 yi = 0U; yi < 2U; ++yi) {
                for (u32 zi = 0U; zi < 2U; ++zi) {
                    const auto p4 =
                        Core::Math::MatMul(world, FVector4f(xs[xi], ys[yi], zs[zi], 1.0f));
                    const f32 invW = (Abs(p4[3]) > 1e-6f) ? (1.0f / p4[3]) : 1.0f;
                    const f32 x    = p4[0] * invW;
                    const f32 y    = p4[1] * invW;
                    const f32 z    = p4[2] * invW;
                    minWS[0]       = Min(minWS[0], x);
                    minWS[1]       = Min(minWS[1], y);
                    minWS[2]       = Min(minWS[2], z);
                    maxWS[0]       = Max(maxWS[0], x);
                    maxWS[1]       = Max(maxWS[1], y);
                    maxWS[2]       = Max(maxWS[2], z);
                }
            }
        }

        outMinWS = minWS;
        outMaxWS = maxWS;
        return true;
    }
} // namespace AltinaEngine::Core::Math::LinAlg