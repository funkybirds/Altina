#pragma once

#include "Container/HashMap.h"
#include "Container/Vector.h"
#include "DebugGui/Core/Types.h"
#include "Types/Aliases.h"

namespace AltinaEngine::DebugGui::Private {
    struct FIconAtlasEntry {
        DebugGui::FDebugGuiIconId mIconId    = DebugGui::kInvalidDebugGuiIconId;
        u32                       mCellIndex = 0U;
    };

    struct FIconAtlas {
        static constexpr u32                                                  kAtlasIconW    = 64U;
        static constexpr u32                                                  kAtlasIconH    = 64U;
        static constexpr f32                                                  kSdfPixelRange = 6.0f;

        u32                                                                   mCols   = 1U;
        u32                                                                   mRows   = 1U;
        u32                                                                   mAtlasW = kAtlasIconW;
        u32                                                                   mAtlasH = kAtlasIconH;
        Core::Container::TVector<u8>                                          mPixels;
        Core::Container::THashMap<DebugGui::FDebugGuiIconId, FIconAtlasEntry> mEntries;

        void                                                                  Build();
        [[nodiscard]] auto TryGetIconUV(DebugGui::FDebugGuiIconId iconId, f32& outU0, f32& outV0,
            f32& outU1, f32& outV1) const noexcept -> bool;
    };
} // namespace AltinaEngine::DebugGui::Private
