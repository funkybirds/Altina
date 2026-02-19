#pragma once

#include "Rendering/RenderingAPI.h"

#include "Render/DrawList.h"
#include "Rhi/RhiFwd.h"

namespace AltinaEngine::Rendering {
    struct FDrawListBindings {
        Rhi::FRhiBindGroup* PerFrame            = nullptr;
        u32                 PerFrameSetIndex    = 0U;
        u32                 PerDrawSetIndex     = 1U;
        u32                 PerMaterialSetIndex = 2U;
    };

    using FDrawPipelineResolver =
        Rhi::FRhiPipeline* (*)(const RenderCore::Render::FDrawBatch& batch,
            const RenderCore::FMaterialPassDesc* passDesc, void* userData);
    using FDrawBatchBinder = void (*)(
        Rhi::FRhiCmdContext& ctx, const RenderCore::Render::FDrawBatch& batch, void* userData);

    class AE_RENDERING_API FDrawListExecutor {
    public:
        static void ExecuteBasePass(Rhi::FRhiCmdContext& ctx,
            const RenderCore::Render::FDrawList& drawList, const FDrawListBindings& bindings,
            FDrawPipelineResolver pipelineResolver = nullptr, void* pipelineUserData = nullptr,
            FDrawBatchBinder batchBinder = nullptr, void* batchUserData = nullptr);
    };
} // namespace AltinaEngine::Rendering
