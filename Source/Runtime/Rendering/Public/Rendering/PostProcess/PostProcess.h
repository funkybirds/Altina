#pragma once

#include "Rendering/RenderingAPI.h"

#include "Container/HashMap.h"
#include "Container/String.h"
#include "Container/Variant.h"
#include "Container/Vector.h"
#include "FrameGraph/FrameGraph.h"
#include "Math/Vector.h"
#include "Rhi/RhiEnums.h"
#include "Types/Aliases.h"

namespace AltinaEngine::RenderCore::View {
    struct FViewData;
}

namespace AltinaEngine::Rendering {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::FStringView;
    using Container::THashMap;
    using Container::TVariant;
    using Container::TVector;

    struct AE_RENDERING_API FPostProcessIO {
        RenderCore::FFrameGraphTextureRef
            SceneColor;                          // HDR/LDR depending on where you are in the chain
        RenderCore::FFrameGraphTextureRef Depth; // Optional
    };

    using FPostProcessParamValue = TVariant<bool, i32, f32, Core::Math::FVector2f,
        Core::Math::FVector3f, Core::Math::FVector4f, FString>;

    using FPostProcessParams = THashMap<FString, FPostProcessParamValue>;

    struct AE_RENDERING_API FPostProcessNode {
        FString            EffectId;
        bool               bEnabled = true;
        FPostProcessParams Params;
    };

    struct AE_RENDERING_API FPostProcessStack {
        bool                      bEnable = true;
        TVector<FPostProcessNode> Stack;
    };

    struct AE_RENDERING_API FPostProcessBuildContext {
        RenderCore::FFrameGraphTextureRef BackBuffer;
        Rhi::ERhiFormat                   BackBufferFormat = Rhi::ERhiFormat::Unknown;
    };

    using FPostProcessAddToGraphFn = void (*)(RenderCore::FFrameGraph& graph,
        const RenderCore::View::FViewData& view, const FPostProcessNode& node,
        const FPostProcessBuildContext& ctx, FPostProcessIO& io);

    /**
     * Register (or replace) an effect implementation for an EffectId.
     *
     * @return true if effectId is non-empty and fn is non-null.
     */
    AE_RENDERING_API auto RegisterPostProcessEffect(
        FStringView effectId, FPostProcessAddToGraphFn fn) noexcept -> bool;

    /**
     * @return true if effectId existed and got removed.
     */
    AE_RENDERING_API auto UnregisterPostProcessEffect(FStringView effectId) noexcept -> bool;

    /**
     * Build a post-process chain from (stack + effect registry), then always append a final Present
     * pass that writes to ctx.BackBuffer and marks it as ExternalOutput(Present).
     *
     * Notes:
     * - Unregistered effects are skipped (logged once).
     * - Effects should treat io.SceneColor as "current color" and replace it with the newly
     * produced texture.
     */
    AE_RENDERING_API void BuildPostProcess(RenderCore::FFrameGraph& graph,
        const RenderCore::View::FViewData& view, const FPostProcessStack& stack, FPostProcessIO& io,
        const FPostProcessBuildContext& ctx);
} // namespace AltinaEngine::Rendering
