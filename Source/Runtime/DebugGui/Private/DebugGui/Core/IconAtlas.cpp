#include "DebugGui/Core/IconAtlas.h"

namespace AltinaEngine::DebugGui::Private {
#include "DebugGui/EditorIconAtlasSDF64.inl"

    void FIconAtlas::Build() {
        mCols   = GetEditorIconAtlasCols();
        mRows   = GetEditorIconAtlasRows();
        mAtlasW = mCols * kAtlasIconW;
        mAtlasH = mRows * kAtlasIconH;

        const u32   pixelCount = mAtlasW * mAtlasH * 4U;
        const u8*   srcPixels  = GetEditorIconAtlasPixels();
        const usize srcCount   = static_cast<usize>(mAtlasW) * static_cast<usize>(mAtlasH);

        mPixels.Resize(pixelCount);
        for (usize index = 0; index < srcCount; ++index) {
            const usize dst   = index * 4U;
            const u8    sdf   = srcPixels[index];
            mPixels[dst + 0U] = sdf;
            mPixels[dst + 1U] = sdf;
            mPixels[dst + 2U] = sdf;
            mPixels[dst + 3U] = 255U;
        }

        mEntries.Clear();
        const u32 iconCount = GetEditorIconAtlasIconCount();
        for (u32 index = 0U; index < iconCount; ++index) {
            FIconAtlasEntry entry{};
            entry.mIconId           = GetEditorIconAtlasIconId(index);
            entry.mCellIndex        = index;
            mEntries[entry.mIconId] = entry;
        }
    }

    auto FIconAtlas::TryGetIconUV(DebugGui::FDebugGuiIconId iconId, f32& outU0, f32& outV0,
        f32& outU1, f32& outV1) const noexcept -> bool {
        auto it = mEntries.FindIt(iconId);
        if (it == mEntries.end() || mCols == 0U || mAtlasW == 0U || mAtlasH == 0U) {
            return false;
        }

        const u32 cellIndex = it->second.mCellIndex;
        const u32 col       = cellIndex % mCols;
        const u32 row       = cellIndex / mCols;
        const f32 invW      = 1.0f / static_cast<f32>(mAtlasW);
        const f32 invH      = 1.0f / static_cast<f32>(mAtlasH);
        const f32 x0        = static_cast<f32>(col * kAtlasIconW);
        const f32 y0        = static_cast<f32>(row * kAtlasIconH);
        const f32 x1        = x0 + static_cast<f32>(kAtlasIconW);
        const f32 y1        = y0 + static_cast<f32>(kAtlasIconH);

        outU0 = (x0 + 0.5f) * invW;
        outV0 = (y0 + 0.5f) * invH;
        outU1 = (x1 - 0.5f) * invW;
        outV1 = (y1 - 0.5f) * invH;
        return true;
    }
} // namespace AltinaEngine::DebugGui::Private
