#pragma once

#include "Math/Common.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Math/LinAlg/Common.h"

namespace AltinaEngine::Core::Math::LinAlg {
    namespace {
        [[nodiscard]] AE_FORCEINLINE auto Dot(const FVector3f& a, const FVector3f& b) noexcept
            -> f32 {
            return a.X() * b.X() + a.Y() * b.Y() + a.Z() * b.Z();
        }

        [[nodiscard]] AE_FORCEINLINE auto Cross(const FVector3f& a, const FVector3f& b) noexcept
            -> FVector3f {
            return FVector3f(a.Y() * b.Z() - a.Z() * b.Y(), a.Z() * b.X() - a.X() * b.Z(),
                a.X() * b.Y() - a.Y() * b.X());
        }

        [[nodiscard]] AE_FORCEINLINE auto Normalize(const FVector3f& v) noexcept -> FVector3f {
            const f32 lenSq = Dot(v, v);
            if (lenSq <= 0.0f) {
                return FVector3f(0.0f);
            }
            const f32 invLen = 1.0f / Sqrt(lenSq);
            return FVector3f(v.X() * invLen, v.Y() * invLen, v.Z() * invLen);
        }
    } // namespace

    [[nodiscard]] AE_FORCEINLINE auto LookAtLH(const FVector3f& eye, const FVector3f& target,
        const FVector3f& up) noexcept -> FMatrix4x4f {
        const FVector3f forward = Normalize(target - eye);
        const FVector3f right   = Normalize(Cross(up, forward));
        const FVector3f upAxis  = Cross(forward, right);

        if (Dot(right, right) <= 0.0f || Dot(forward, forward) <= 0.0f) {
            return Identity<f32, 4>();
        }

        FMatrix4x4f out(0.0f);
        out(0, 0) = right.X();
        out(0, 1) = right.Y();
        out(0, 2) = right.Z();
        out(0, 3) = -Dot(right, eye);

        out(1, 0) = upAxis.X();
        out(1, 1) = upAxis.Y();
        out(1, 2) = upAxis.Z();
        out(1, 3) = -Dot(upAxis, eye);

        out(2, 0) = forward.X();
        out(2, 1) = forward.Y();
        out(2, 2) = forward.Z();
        out(2, 3) = -Dot(forward, eye);

        out(3, 3) = 1.0f;
        return out;
    }
} // namespace AltinaEngine::Core::Math::LinAlg
