#pragma once
#include "Types/Aliases.h"
#include "../Common.h"
#include "../Vector.h"

namespace AltinaEngine::Core::Math {
    inline auto ConcentricOctahedralTransform(const FVector2f& sample) -> FVector3f {
        // https://zhuanlan.zhihu.com/p/408898601
        // https://fileadmin.cs.lth.se/graphics/research/papers/2008/simdmapping/clarberg_simdmapping08_preprint.pdf

        f32       sx = sample.X();
        f32       sy = sample.Y();
        FVector2f sampleOffset(sx * 2.0f - 1.0f, sy * 2.0f - 1.0f);
        if (sampleOffset.X() == 0.0f && sampleOffset.Y() == 0.0f) {
            return FVector3f(0.0f);
        }

        f32 u = sampleOffset.X();
        f32 v = sampleOffset.Y();
        f32 d = 1.0f - Abs(u) - Abs(v);
        f32 r = 1.0f - Abs(d);

        f32 z     = ((d > 0.0f) ? 1.0f : -1.0f) * (1.0f - r * r);
        f32 theta = kPiF / 4.0f * ((Abs(v) - Abs(u)) / r + 1.0f);
        f32 sinT  = Sin(theta) * ((v >= 0.0f) ? 1.0f : -1.0f);
        f32 cosT  = Cos(theta) * ((u >= 0.0f) ? 1.0f : -1.0f);
        f32 x     = cosT * r * Sqrt(2.0f - z * z);
        f32 y     = sinT * r * Sqrt(2.0f - z * z);
        return FVector3f(x, y, z);
    }
} // namespace AltinaEngine::Core::Math