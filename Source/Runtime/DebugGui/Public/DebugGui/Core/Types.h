#pragma once

#include "Container/StringView.h"
#include "Math/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::DebugGui {
    using Core::Container::FStringView;
    using Core::Math::FVector2f;

    struct FRect {
        FVector2f Min = FVector2f(0.0f, 0.0f);
        FVector2f Max = FVector2f(0.0f, 0.0f);
    };

    // Packed RGBA8 in little-endian memory order: R | (G<<8) | (B<<16) | (A<<24).
    using FColor32 = u32;

    [[nodiscard]] constexpr auto MakeColor32(u8 r, u8 g, u8 b, u8 a = 255) noexcept -> FColor32 {
        return static_cast<FColor32>(r) | (static_cast<FColor32>(g) << 8U)
            | (static_cast<FColor32>(b) << 16U) | (static_cast<FColor32>(a) << 24U);
    }

    struct FDebugGuiFrameStats {
        u32 mVertexCount = 0U;
        u32 mIndexCount  = 0U;
        u32 mCmdCount    = 0U;
    };

    struct FDebugGuiExternalStats {
        u64 mFrameIndex      = 0ULL;
        u32 mViewCount       = 0U;
        u32 mSceneBatchCount = 0U;
    };
} // namespace AltinaEngine::DebugGui
