#pragma once

#include "Container/HashMap.h"
#include "Rhi/RhiEnums.h"
#include "Rhi/RhiRefs.h"
#include "Types/Aliases.h"
#include "View/ViewData.h"

namespace AltinaEngine::Rhi {
    class FRhiDevice;
}

namespace AltinaEngine::Rendering::TemporalAA::Detail {
    namespace Container = Core::Container;
    using Container::THashMap;

    struct FTemporalAAViewState {
        bool                bValidHistory = false;
        u32                 Width         = 0U;
        u32                 Height        = 0U;
        Rhi::ERhiFormat     HistoryFormat = Rhi::ERhiFormat::Unknown;

        // Ping-pong history textures (external RHI textures).
        Rhi::FRhiTextureRef HistoryA;
        Rhi::FRhiTextureRef HistoryB;
        bool                bHistoryToggle = false; // false: A is read, true: B is read

        RenderCore::View::FPreviousViewData PrevView{};

        // Temporal jitter sample index (advanced per rendered frame).
        u32                                 TemporalSampleIndex = 0U;

        void                                InvalidateHistory() noexcept {
            bValidHistory = false;
            PrevView.Invalidate();
        }

        void ResetTemporalIndex() noexcept { TemporalSampleIndex = 0U; }
    };

    [[nodiscard]] auto GetOrCreateViewState(u64 viewKey) -> FTemporalAAViewState&;

    // Ensure external history textures are allocated and match size/format.
    void               EnsureHistoryTextures(
                      Rhi::FRhiDevice& device, u64 viewKey, u32 width, u32 height, Rhi::ERhiFormat format);

    // Called after the TAA pass writes HistoryWrite to make it the read texture for next frame.
    void               CommitHistoryWritten(u64 viewKey) noexcept;

    [[nodiscard]] auto GetHistoryReadTexture(u64 viewKey) -> Rhi::FRhiTextureRef;
    [[nodiscard]] auto GetHistoryWriteTexture(u64 viewKey) -> Rhi::FRhiTextureRef;
} // namespace AltinaEngine::Rendering::TemporalAA::Detail
