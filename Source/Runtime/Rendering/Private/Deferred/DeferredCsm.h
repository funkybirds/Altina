#pragma once

#include "Deferred/DeferredTypes.h"

#include "Lighting/LightTypes.h"
#include "Render/DrawList.h"
#include "Shadow/CascadedShadowMapping.h"
#include "View/ViewData.h"
#include "FrameGraph/FrameGraph.h"

#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiRefs.h"

namespace AltinaEngine::Rendering::Deferred {
    struct FCsmBuildInputs {
        const RenderCore::View::FViewData*           View           = nullptr;
        const RenderCore::Lighting::FLightSceneData* Lights         = nullptr;
        const RenderCore::Render::FDrawList*         ShadowDrawList = nullptr;
    };

    struct FCsmBuildResult {
        RenderCore::Shadow::FCSMSettings Settings{};
        RenderCore::Shadow::FCSMData     Data{};
    };

    [[nodiscard]] auto BuildCsm(const FCsmBuildInputs& inputs) -> FCsmBuildResult;
    void               FillPerFrameCsmConstants(
                      const FCsmBuildResult& csm, FPerFrameConstants& outPerFrameConstants);

    using FCsmExecuteCascadeFn = void (*)(Rhi::FRhiCmdContext& ctx, u32 cascadeIndex,
        const Core::Math::FMatrix4x4f& lightViewProj, u32 shadowMapSize, void* userData);

    struct FCsmShadowPassInputs {
        RenderCore::FFrameGraph*             Graph          = nullptr;
        const RenderCore::View::FViewData*   View           = nullptr;
        const RenderCore::Render::FDrawList* ShadowDrawList = nullptr;
        const FCsmBuildResult*               Csm            = nullptr;

        Rhi::FRhiTextureRef*                 PersistentShadowMap       = nullptr;
        u32*                                 PersistentShadowMapSize   = nullptr;
        u32*                                 PersistentShadowMapLayers = nullptr;

        FCsmExecuteCascadeFn                 ExecuteCascadeFn       = nullptr;
        void*                                ExecuteCascadeUserData = nullptr;
    };

    void AddCsmShadowPasses(
        FCsmShadowPassInputs& inputs, RenderCore::FFrameGraphTextureRef& outShadowMap);
} // namespace AltinaEngine::Rendering::Deferred
