#pragma once

#include "RhiD3D11API.h"
#include "Rhi/RhiCommandContext.h"
#include "Rhi/RhiRefs.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace AltinaEngine::Rhi {
    class AE_RHI_D3D11_API FRhiD3D11CommandContext final : public FRhiCommandContext {
    public:
        FRhiD3D11CommandContext(const FRhiCommandContextDesc& desc, ID3D11Device* device,
            FRhiCommandListRef commandList);
        ~FRhiD3D11CommandContext() override;

        void Begin() override;
        void End() override;
        [[nodiscard]] auto GetCommandList() const noexcept -> FRhiCommandList* override;

        [[nodiscard]] auto GetDeferredContext() const noexcept -> ID3D11DeviceContext*;

    private:
        struct FState;
        FState* mState = nullptr;
        FRhiCommandListRef mCommandList;
    };

} // namespace AltinaEngine::Rhi
