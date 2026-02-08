#pragma once

#include "RhiD3D11API.h"
#include "Rhi/RhiCommandContext.h"
#include "Rhi/Command/RhiCmdContextOps.h"
#include "Rhi/RhiRefs.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace AltinaEngine::Rhi {
    class AE_RHI_D3D11_API FRhiD3D11CommandContext final : public FRhiCommandContext,
                                                           public IRhiCmdContextOps {
    public:
        FRhiD3D11CommandContext(const FRhiCommandContextDesc& desc, ID3D11Device* device,
            FRhiCommandListRef commandList);
        ~FRhiD3D11CommandContext() override;

        void Begin() override;
        void End() override;
        [[nodiscard]] auto GetCommandList() const noexcept -> FRhiCommandList* override;

        void RHISetGraphicsPipeline(FRhiPipeline* pipeline) override;
        void RHISetComputePipeline(FRhiPipeline* pipeline) override;
        void RHISetRenderTargets(u32 colorTargetCount, FRhiTexture* const* colorTargets,
            FRhiTexture* depthTarget) override;
        void RHISetBindGroup(u32 setIndex, FRhiBindGroup* group, const u32* dynamicOffsets,
            u32 dynamicOffsetCount) override;
        void RHIDrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex,
            i32 vertexOffset, u32 firstInstance) override;
        void RHIDispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) override;

        [[nodiscard]] auto GetDeferredContext() const noexcept -> ID3D11DeviceContext*;

    private:
        struct FState;
        FState* mState = nullptr;
        FRhiCommandListRef mCommandList;
    };

} // namespace AltinaEngine::Rhi
