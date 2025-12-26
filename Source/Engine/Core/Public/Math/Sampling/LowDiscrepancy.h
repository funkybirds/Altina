#pragma once
#include "Types/Aliases.h"
#include "../Common.h"
#include "../Vector.h"

namespace AltinaEngine::Core::Math
{
    [[nodiscard]] inline constexpr f32 VanDeCorputRadicalInverse2(u32 bits) noexcept
    {
        // Reference:
        // https://github.com/Nadrin/PBR/blob/master/data/shaders/hlsl/spmap.hlsl
        // https://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html

        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return static_cast<f32>(f64(bits) * 2.3283064365386963e-10); // / 0x100000000
    }

    [[nodiscard]] AE_FORCEINLINE constexpr FVector2f Hammersley2d(u32 x, u32 N) noexcept
    {
        f32 u1 = f32(x) / f32(N);
        f32 u2 = VanDeCorputRadicalInverse2(x);
        return FVector2f(u1, u2);
    }
} // namespace AltinaEngine::Core::Math