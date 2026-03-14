#pragma once

#include "Container/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::DebugGui::Private {
    struct FFontGlyphMetrics {
        f32 mAdvance  = 7.0f;
        f32 mBearingX = 0.0f;
        f32 mBearingY = 0.0f;
    };

    struct FFontAtlas {
        static constexpr u32                        kAtlasGlyphW   = 64U;
        static constexpr u32                        kAtlasGlyphH   = 64U;
        static constexpr u32                        kDrawGlyphW    = 7U;
        static constexpr u32                        kDrawGlyphH    = 11U;
        static constexpr f32                        kSdfPixelRange = 4.0f;
        static constexpr f32                        kSdfEdgeValue  = 0.5f;
        static constexpr u32                        kFirstChar     = 32U;
        static constexpr u32                        kLastChar      = 126U;
        static constexpr u32                        kGlyphCount    = (kLastChar - kFirstChar + 1U);
        static constexpr u32                        kCols          = 16U;
        static constexpr u32                        kRows   = (kGlyphCount + kCols - 1U) / kCols;
        static constexpr u32                        kAtlasW = kCols * kAtlasGlyphW;
        static constexpr u32                        kAtlasH = kRows * kAtlasGlyphH;
        static constexpr u32                        kSolidTexelX = (kCols - 1U) * kAtlasGlyphW;
        static constexpr u32                        kSolidTexelY = (kRows - 1U) * kAtlasGlyphH;

        Core::Container::TVector<u8>                mPixels;
        Core::Container::TVector<FFontGlyphMetrics> mGlyphMetrics;
        f32                                         mRecommendedStretchX = 1.0f;

        void                                        Build();
        void GetGlyphUV(u32 ch, f32& outU0, f32& outV0, f32& outU1, f32& outV1) const noexcept;
        [[nodiscard]] static auto GetGlyphWidth(f32 fontScale) noexcept -> f32;
        [[nodiscard]] static auto GetGlyphHeight(f32 fontScale) noexcept -> f32;
        [[nodiscard]] auto        GetGlyphMetrics(u32 ch) const noexcept -> FFontGlyphMetrics;
        [[nodiscard]] auto        GetRecommendedStretchX() const noexcept -> f32 {
            return mRecommendedStretchX;
        }
    };
} // namespace AltinaEngine::DebugGui::Private
