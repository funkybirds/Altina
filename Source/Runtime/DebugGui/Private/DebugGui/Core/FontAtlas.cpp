#include "DebugGui/Core/FontAtlas.h"

#include <cstddef>

namespace AltinaEngine::DebugGui::Private {
#include "DebugGui/FontAtlasMSDF64x64.inl"

    auto ResolveFontRoleScale(const FDebugGuiTheme& theme, EDebugGuiFontRole role) noexcept -> f32 {
        const auto ResolveScale = [](const FDebugGuiFontStyle& style) {
            return (style.mScale > 0.01f) ? style.mScale : 1.0f;
        };
        switch (role) {
            case EDebugGuiFontRole::Small:
                return ResolveScale(theme.mFonts.mSmall);
            case EDebugGuiFontRole::WindowTitle:
                return ResolveScale(theme.mFonts.mWindowTitle);
            case EDebugGuiFontRole::Menu:
                return ResolveScale(theme.mFonts.mMenu);
            case EDebugGuiFontRole::Tab:
                return ResolveScale(theme.mFonts.mTab);
            case EDebugGuiFontRole::Section:
                return ResolveScale(theme.mFonts.mSection);
            case EDebugGuiFontRole::Label:
                return ResolveScale(theme.mFonts.mLabel);
            case EDebugGuiFontRole::Status:
                return ResolveScale(theme.mFonts.mStatus);
            case EDebugGuiFontRole::Body:
            default:
                return ResolveScale(theme.mFonts.mBody);
        }
    }

    void FFontAtlas::Build() {
        mPixels.Resize(static_cast<usize>(kAtlasW) * static_cast<usize>(kAtlasH) * 4U);
        mGlyphMetrics.Resize(static_cast<usize>(kGlyphCount));
        mRecommendedStretchX = GetFont32x32RecommendedStretchX();
        for (usize i = 0; i < mPixels.Size(); ++i) {
            mPixels[i] = 0U;
        }

        for (u32 ch = kFirstChar; ch <= kLastChar; ++ch) {
            const u32 glyphIndex = ch - kFirstChar;
            const u32 col        = glyphIndex % kCols;
            const u32 row        = glyphIndex / kCols;
            const u32 baseX      = col * kAtlasGlyphW;
            const u32 baseY      = row * kAtlasGlyphH;

            const u8* glyph = GetFont32x32MsdfGlyph(static_cast<u8>(ch));
            for (u32 y = 0U; y < kAtlasGlyphH; ++y) {
                for (u32 x = 0U; x < kAtlasGlyphW; ++x) {
                    const usize src   = (static_cast<usize>(y) * kAtlasGlyphW + x) * 3U;
                    const u32   px    = baseX + x;
                    const u32   py    = baseY + y;
                    const usize idx   = (static_cast<usize>(py) * kAtlasW + px) * 4U;
                    mPixels[idx + 0U] = glyph ? glyph[src + 0U] : 0U;
                    mPixels[idx + 1U] = glyph ? glyph[src + 1U] : 0U;
                    mPixels[idx + 2U] = glyph ? glyph[src + 2U] : 0U;
                    mPixels[idx + 3U] = 255U;
                }
            }

            const u32 mIdx                = ch - kFirstChar;
            mGlyphMetrics[mIdx].mAdvance  = GetFont32x32GlyphAdvance(static_cast<u8>(ch));
            mGlyphMetrics[mIdx].mBearingX = GetFont32x32GlyphBearingX(static_cast<u8>(ch));
            mGlyphMetrics[mIdx].mBearingY = GetFont32x32GlyphBearingY(static_cast<u8>(ch));
        }

        for (u32 y = 0U; y < kAtlasGlyphH; ++y) {
            for (u32 x = 0U; x < kAtlasGlyphW; ++x) {
                const u32   px  = kSolidTexelX + x;
                const u32   py  = kSolidTexelY + y;
                const usize idx = (static_cast<usize>(py) * kAtlasW + px) * 4U;
                if (idx + 3U >= mPixels.Size()) {
                    continue;
                }
                mPixels[idx + 0U] = 255U;
                mPixels[idx + 1U] = 255U;
                mPixels[idx + 2U] = 255U;
                mPixels[idx + 3U] = 255U;
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

    auto FFontAtlas::GetGlyphWidth(f32 fontScale) noexcept -> f32 {
        const f32 safeScale = (fontScale > 0.01f) ? fontScale : 1.0f;
        return static_cast<f32>(kDrawGlyphW) * safeScale;
    }

    auto FFontAtlas::GetGlyphHeight(f32 fontScale) noexcept -> f32 {
        const f32 safeScale = (fontScale > 0.01f) ? fontScale : 1.0f;
        return static_cast<f32>(kDrawGlyphH) * safeScale;
    }

    auto FFontAtlas::GetGlyphWidth(const FDebugGuiTheme& theme, EDebugGuiFontRole role) noexcept
        -> f32 {
        return GetGlyphWidth(ResolveFontRoleScale(theme, role));
    }

    auto FFontAtlas::GetGlyphHeight(const FDebugGuiTheme& theme, EDebugGuiFontRole role) noexcept
        -> f32 {
        return GetGlyphHeight(ResolveFontRoleScale(theme, role));
    }

    auto FFontAtlas::GetGlyphMetrics(u32 ch) const noexcept -> FFontGlyphMetrics {
        if (ch < kFirstChar || ch > kLastChar || mGlyphMetrics.IsEmpty()) {
            return FFontGlyphMetrics{};
        }
        return mGlyphMetrics[ch - kFirstChar];
    }
} // namespace AltinaEngine::DebugGui::Private
