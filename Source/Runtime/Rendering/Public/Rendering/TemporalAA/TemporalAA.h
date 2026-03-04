#pragma once

#include "Rendering/RenderingAPI.h"

#include "Types/Aliases.h"
#include "View/ViewData.h"

namespace AltinaEngine::Rendering::TemporalAA {
    /**
     * Prepare a view for rendering with temporal jitter + previous-frame data.
     *
     * This function is intended to be called on the render thread, before assembling passes.
     * It updates:
     * - view.Previous (from persistent view-state cache)
     * - view.TemporalSampleIndex
     * - view.Matrices via view.BeginFrame(jitterNdc)
     */
    AE_RENDERING_API void PrepareViewForFrame(
        u64 viewKey, RenderCore::View::FViewData& view, bool bEnableJitter, u32 sampleCount);

    /**
     * Persist current frame's view snapshot into the view-state cache.
     *
     * This function is intended to be called on the render thread, after executing the frame graph.
     */
    AE_RENDERING_API void FinalizeViewForFrame(
        u64 viewKey, const RenderCore::View::FViewData& view, bool bEnableJitter, u32 sampleCount);

    // Release persistent temporal history textures/state.
    AE_RENDERING_API void ShutdownTemporalAA() noexcept;
} // namespace AltinaEngine::Rendering::TemporalAA
