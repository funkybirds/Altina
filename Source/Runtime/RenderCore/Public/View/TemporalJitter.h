#pragma once

#include "Math/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::RenderCore::View {
    /**
     * Deterministic camera jitter helpers for temporal sampling.
     *
     * - Halton(2,3) sequence is cheap and good enough for first-iteration TAA.
     * - Jitter is returned in pixel units in [-0.5, 0.5], and can be converted to NDC.
     */

    [[nodiscard]] constexpr auto HaltonSequence(u32 index, u32 base) noexcept -> f32 {
        // index is 1-based for conventional Halton sequences.
        f32 result = 0.0f;
        f32 f      = 1.0f / static_cast<f32>(base);
        while (index > 0U) {
            result += f * static_cast<f32>(index % base);
            index /= base;
            f *= 1.0f / static_cast<f32>(base);
        }
        return result;
    }

    [[nodiscard]] constexpr auto ComputeHalton23JitterPx(
        u32 temporalSampleIndex, u32 sampleCount) noexcept -> Core::Math::FVector2f {
        if (sampleCount == 0U) {
            sampleCount = 1U;
        }

        // Keep jitter in [-0.5, 0.5] px.
        const u32 idx = (temporalSampleIndex % sampleCount) + 1U;
        const f32 jx  = HaltonSequence(idx, 2U) - 0.5f;
        const f32 jy  = HaltonSequence(idx, 3U) - 0.5f;
        return Core::Math::FVector2f(jx, jy);
    }

    [[nodiscard]] constexpr auto JitterPxToNdc(const Core::Math::FVector2f& jitterPx, u32 width,
        u32 height) noexcept -> Core::Math::FVector2f {
        // Convert pixel offset to NDC offset.
        // See Spec.DeferredTAA.md:
        // jitterNdc.x = (2 * jitterPx.x) / width
        // jitterNdc.y = (2 * jitterPx.y) / height
        const f32 w = static_cast<f32>(width);
        const f32 h = static_cast<f32>(height);
        const f32 x = (w > 0.0f) ? ((2.0f * jitterPx[0]) / w) : 0.0f;
        const f32 y = (h > 0.0f) ? ((2.0f * jitterPx[1]) / h) : 0.0f;
        return Core::Math::FVector2f(x, y);
    }

    [[nodiscard]] constexpr auto ComputeHalton23JitterNdc(u32 temporalSampleIndex, u32 sampleCount,
        u32 width, u32 height) noexcept -> Core::Math::FVector2f {
        return JitterPxToNdc(
            ComputeHalton23JitterPx(temporalSampleIndex, sampleCount), width, height);
    }
} // namespace AltinaEngine::RenderCore::View
