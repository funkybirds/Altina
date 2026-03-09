#pragma once

#include "Math/Common.h"
#include "Math/Vector.h"

namespace AltinaEngine::Core::Math {
    [[nodiscard]] AE_FORCEINLINE auto Dot2(const FVector2f& a, const FVector2f& b) noexcept -> f32 {
        return a.X() * b.X() + a.Y() * b.Y();
    }

    [[nodiscard]] AE_FORCEINLINE auto DistPointSegmentSq(
        const FVector2f& p, const FVector2f& a, const FVector2f& b) noexcept -> f32 {
        const FVector2f ab(b.X() - a.X(), b.Y() - a.Y());
        const FVector2f ap(p.X() - a.X(), p.Y() - a.Y());
        const f32       abLenSq = Dot2(ab, ab);
        if (abLenSq <= 0.0001f) {
            const f32 dx = p.X() - a.X();
            const f32 dy = p.Y() - a.Y();
            return dx * dx + dy * dy;
        }

        const f32 t  = Clamp(Dot2(ap, ab) / abLenSq, 0.0f, 1.0f);
        const f32 qx = a.X() + ab.X() * t;
        const f32 qy = a.Y() + ab.Y() * t;
        const f32 dx = p.X() - qx;
        const f32 dy = p.Y() - qy;
        return dx * dx + dy * dy;
    }
} // namespace AltinaEngine::Core::Math
