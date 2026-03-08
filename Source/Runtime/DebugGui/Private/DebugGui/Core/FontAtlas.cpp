#include "DebugGui/Core/FontAtlas.h"

#include <cstddef>

namespace AltinaEngine::DebugGui::Private {
#include "DebugGui/FontAtlas32x32.inl"

    void FFontAtlas::Build() {
        Pixels.Resize(static_cast<usize>(kAtlasW) * static_cast<usize>(kAtlasH) * 4U);
        for (usize i = 0; i < Pixels.Size(); ++i) {
            Pixels[i] = 0U;
        }

        for (u32 ch = kFirstChar; ch <= kLastChar; ++ch) {
            const u32 glyphIndex = ch - kFirstChar;
            const u32 col        = glyphIndex % kCols;
            const u32 row        = glyphIndex / kCols;
            const u32 baseX      = col * kAtlasGlyphW;
            const u32 baseY      = row * kAtlasGlyphH;

            const u8* glyph = GetFont32x32Glyph(static_cast<u8>(ch));
            for (u32 y = 0U; y < kAtlasGlyphH; ++y) {
                for (u32 x = 0U; x < kAtlasGlyphW; ++x) {
                    const u8    a    = glyph ? glyph[y * kAtlasGlyphW + x] : 0U;
                    const u32   px   = baseX + x;
                    const u32   py   = baseY + y;
                    const usize idx  = (static_cast<usize>(py) * kAtlasW + px) * 4U;
                    Pixels[idx + 0U] = 255U;
                    Pixels[idx + 1U] = 255U;
                    Pixels[idx + 2U] = 255U;
                    Pixels[idx + 3U] = a;
                }
            }
        }

        for (u32 y = 0U; y < kAtlasGlyphH; ++y) {
            for (u32 x = 0U; x < kAtlasGlyphW; ++x) {
                const u32   px  = kSolidTexelX + x;
                const u32   py  = kSolidTexelY + y;
                const usize idx = (static_cast<usize>(py) * kAtlasW + px) * 4U;
                if (idx + 3U >= Pixels.Size()) {
                    continue;
                }
                Pixels[idx + 0U] = 255U;
                Pixels[idx + 1U] = 255U;
                Pixels[idx + 2U] = 255U;
                Pixels[idx + 3U] = 255U;
            }
        }
    }

    void FFontAtlas::GetGlyphUV(
        u32 ch, f32& outU0, f32& outV0, f32& outU1, f32& outV1) const noexcept {
        if (ch < kFirstChar || ch > kLastChar) {
            ch = static_cast<u32>('?');
        }
        const u32 glyphIndex = ch - kFirstChar;
        const u32 col        = glyphIndex % kCols;
        const u32 row        = glyphIndex / kCols;
        const f32 invW       = 1.0f / static_cast<f32>(kAtlasW);
        const f32 invH       = 1.0f / static_cast<f32>(kAtlasH);
        const f32 x0         = static_cast<f32>(col * kAtlasGlyphW);
        const f32 y0         = static_cast<f32>(row * kAtlasGlyphH);
        const f32 x1         = x0 + static_cast<f32>(kAtlasGlyphW);
        const f32 y1         = y0 + static_cast<f32>(kAtlasGlyphH);

        outU0 = (x0 + 0.5f) * invW;
        outV0 = (y0 + 0.5f) * invH;
        outU1 = (x1 - 0.5f) * invW;
        outV1 = (y1 - 0.5f) * invH;
    }
} // namespace AltinaEngine::DebugGui::Private
