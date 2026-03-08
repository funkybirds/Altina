#pragma once

#include "Container/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::DebugGui::Private {
    struct FFontAtlas {
        static constexpr u32         kAtlasGlyphW = 32U;
        static constexpr u32         kAtlasGlyphH = 32U;
        static constexpr u32         kDrawGlyphW  = 7U;
        static constexpr u32         kDrawGlyphH  = 11U;
        static constexpr u32         kFirstChar   = 32U;
        static constexpr u32         kLastChar    = 126U;
        static constexpr u32         kGlyphCount  = (kLastChar - kFirstChar + 1U);
        static constexpr u32         kCols        = 16U;
        static constexpr u32         kRows        = (kGlyphCount + kCols - 1U) / kCols;
        static constexpr u32         kAtlasW      = kCols * kAtlasGlyphW;
        static constexpr u32         kAtlasH      = kRows * kAtlasGlyphH;
        static constexpr u32         kSolidTexelX = (kCols - 1U) * kAtlasGlyphW;
        static constexpr u32         kSolidTexelY = (kRows - 1U) * kAtlasGlyphH;

        Core::Container::TVector<u8> Pixels;

        void                         Build();
        void GetGlyphUV(u32 ch, f32& outU0, f32& outV0, f32& outU1, f32& outV1) const noexcept;
    };
} // namespace AltinaEngine::DebugGui::Private
