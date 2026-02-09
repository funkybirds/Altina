#pragma once

#include "RhiD3D11API.h"
#include "Rhi/RhiViewport.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;

namespace AltinaEngine::Rhi {
    class AE_RHI_D3D11_API FRhiD3D11Viewport final : public FRhiViewport {
    public:
        FRhiD3D11Viewport(const FRhiViewportDesc& desc, ID3D11Device* device,
            ID3D11DeviceContext* immediateContext);
        ~FRhiD3D11Viewport() override;

        void Resize(u32 width, u32 height) override;
        [[nodiscard]] auto GetBackBuffer() const noexcept -> FRhiTexture* override;
        void Present(const FRhiPresentInfo& info) override;

        [[nodiscard]] auto GetSwapChain() const noexcept -> IDXGISwapChain*;
        [[nodiscard]] auto IsValid() const noexcept -> bool;

    private:
        bool CreateSwapChain();
        bool CreateBackBuffer();
        void ReleaseBackBuffer();
        u32 ResolvePresentFlags(u32 syncInterval, u32 flags) const noexcept;

        struct FState;
        FState* mState = nullptr;
    };

} // namespace AltinaEngine::Rhi
