#pragma once

#include "RhiD3D11API.h"
#include "Rhi/RhiCommandList.h"

struct ID3D11CommandList;

namespace AltinaEngine::Rhi {
    class AE_RHI_D3D11_API FRhiD3D11CommandList final : public FRhiCommandList {
    public:
        explicit FRhiD3D11CommandList(const FRhiCommandListDesc& desc);
        ~FRhiD3D11CommandList() override;

        [[nodiscard]] auto GetNativeCommandList() const noexcept -> ID3D11CommandList*;

        void Reset(FRhiCommandPool* pool) override;
        void Close() override;

    private:
        void SetNativeCommandList(ID3D11CommandList* list);

        struct FState;
        FState* mState = nullptr;

        friend class FRhiD3D11CommandContext;
    };

} // namespace AltinaEngine::Rhi
