#include "RhiD3D11/RhiD3D11Device.h"

#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiCommandPool.h"
#include "Rhi/RhiFence.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiSemaphore.h"
#include "Rhi/RhiShader.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"

#if AE_PLATFORM_WIN
    #ifdef TEXT
        #undef TEXT
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
    #ifdef CreateSemaphore
        #undef CreateSemaphore
    #endif
    #include <d3d11.h>
    #include <wrl/client.h>
#endif

#include <type_traits>

namespace AltinaEngine::Rhi {
#if AE_PLATFORM_WIN
    using Microsoft::WRL::ComPtr;

    struct FRhiD3D11Device::FState {
        ComPtr<ID3D11Device>        mDevice;
        ComPtr<ID3D11DeviceContext> mImmediateContext;
        D3D_FEATURE_LEVEL           mFeatureLevel = D3D_FEATURE_LEVEL_11_0;
    };
#else
    struct FRhiD3D11Device::FState {};
#endif

    namespace {
        class FRhiD3D11Shader final : public FRhiShader {
        public:
            explicit FRhiD3D11Shader(const FRhiShaderDesc& desc) : FRhiShader(desc) {}
        };

        class FRhiD3D11GraphicsPipeline final : public FRhiPipeline {
        public:
            explicit FRhiD3D11GraphicsPipeline(const FRhiGraphicsPipelineDesc& desc)
                : FRhiPipeline(desc) {}
        };

        class FRhiD3D11ComputePipeline final : public FRhiPipeline {
        public:
            explicit FRhiD3D11ComputePipeline(const FRhiComputePipelineDesc& desc)
                : FRhiPipeline(desc) {}
        };

        class FRhiD3D11PipelineLayout final : public FRhiPipelineLayout {
        public:
            explicit FRhiD3D11PipelineLayout(const FRhiPipelineLayoutDesc& desc)
                : FRhiPipelineLayout(desc) {}
        };

        class FRhiD3D11BindGroupLayout final : public FRhiBindGroupLayout {
        public:
            explicit FRhiD3D11BindGroupLayout(const FRhiBindGroupLayoutDesc& desc)
                : FRhiBindGroupLayout(desc) {}
        };

        class FRhiD3D11BindGroup final : public FRhiBindGroup {
        public:
            explicit FRhiD3D11BindGroup(const FRhiBindGroupDesc& desc)
                : FRhiBindGroup(desc) {}
        };

        class FRhiD3D11Fence final : public FRhiFence {
        public:
            FRhiD3D11Fence() : FRhiFence() {}
        };

        class FRhiD3D11Semaphore final : public FRhiSemaphore {
        public:
            FRhiD3D11Semaphore() : FRhiSemaphore() {}
        };

        class FRhiD3D11CommandPool final : public FRhiCommandPool {
        public:
            explicit FRhiD3D11CommandPool(const FRhiCommandPoolDesc& desc)
                : FRhiCommandPool(desc) {}
        };

        class FRhiD3D11Queue final : public FRhiQueue {
        public:
            explicit FRhiD3D11Queue(ERhiQueueType type) : FRhiQueue(type) {}

            void Submit(const FRhiSubmitInfo& /*info*/) override {}
            void WaitIdle() override {}
            void Present(const FRhiPresentInfo& /*info*/) override {}
        };
    } // namespace

    FRhiD3D11Device::FRhiD3D11Device(const FRhiDeviceDesc& desc,
        const FRhiAdapterDesc& adapterDesc, ID3D11Device* device,
        ID3D11DeviceContext* context, u32 featureLevel)
        : FRhiDevice(desc, adapterDesc) {
        mState = new FState{};

#if AE_PLATFORM_WIN
        if (mState) {
            mState->mDevice.Attach(device);
            mState->mImmediateContext.Attach(context);
            mState->mFeatureLevel = static_cast<D3D_FEATURE_LEVEL>(featureLevel);
        }

        FRhiSupportedLimits limits;
        limits.mMaxTextureDimension1D     = D3D11_REQ_TEXTURE1D_U_DIMENSION;
        limits.mMaxTextureDimension2D     = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        limits.mMaxTextureDimension3D     = D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
        limits.mMaxTextureArrayLayers     = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
        limits.mMaxSamplers               = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
        limits.mMaxColorAttachments       = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
        SetSupportedLimits(limits);
#else
        (void)device;
        (void)context;
        (void)featureLevel;
#endif

        RegisterQueue(ERhiQueueType::Graphics,
            AdoptResource(new FRhiD3D11Queue(ERhiQueueType::Graphics)));
        RegisterQueue(ERhiQueueType::Compute,
            AdoptResource(new FRhiD3D11Queue(ERhiQueueType::Compute)));
        RegisterQueue(ERhiQueueType::Copy,
            AdoptResource(new FRhiD3D11Queue(ERhiQueueType::Copy)));
    }

    FRhiD3D11Device::~FRhiD3D11Device() {
        delete mState;
        mState = nullptr;
    }

    auto FRhiD3D11Device::GetNativeDevice() const noexcept -> ID3D11Device* {
#if AE_PLATFORM_WIN
        return mState ? mState->mDevice.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Device::GetImmediateContext() const noexcept -> ID3D11DeviceContext* {
#if AE_PLATFORM_WIN
        return mState ? mState->mImmediateContext.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Device::GetFeatureLevel() const noexcept -> u32 {
#if AE_PLATFORM_WIN
        return mState ? static_cast<u32>(mState->mFeatureLevel) : 0U;
#else
        return 0U;
#endif
    }

    auto FRhiD3D11Device::CreateShader(const FRhiShaderDesc& desc) -> FRhiShaderRef {
        return AdoptResource(new FRhiD3D11Shader(desc));
    }

    auto FRhiD3D11Device::CreateGraphicsPipeline(const FRhiGraphicsPipelineDesc& desc)
        -> FRhiPipelineRef {
        return AdoptResource(new FRhiD3D11GraphicsPipeline(desc));
    }

    auto FRhiD3D11Device::CreateComputePipeline(const FRhiComputePipelineDesc& desc)
        -> FRhiPipelineRef {
        return AdoptResource(new FRhiD3D11ComputePipeline(desc));
    }

    auto FRhiD3D11Device::CreatePipelineLayout(const FRhiPipelineLayoutDesc& desc)
        -> FRhiPipelineLayoutRef {
        return AdoptResource(new FRhiD3D11PipelineLayout(desc));
    }

    auto FRhiD3D11Device::CreateBindGroupLayout(const FRhiBindGroupLayoutDesc& desc)
        -> FRhiBindGroupLayoutRef {
        return AdoptResource(new FRhiD3D11BindGroupLayout(desc));
    }

    auto FRhiD3D11Device::CreateBindGroup(const FRhiBindGroupDesc& desc)
        -> FRhiBindGroupRef {
        return AdoptResource(new FRhiD3D11BindGroup(desc));
    }

    auto FRhiD3D11Device::CreateFence(bool /*signaled*/) -> FRhiFenceRef {
        return AdoptResource(new FRhiD3D11Fence());
    }

    auto FRhiD3D11Device::CreateSemaphore() -> FRhiSemaphoreRef {
        return AdoptResource(new FRhiD3D11Semaphore());
    }

    auto FRhiD3D11Device::CreateCommandPool(const FRhiCommandPoolDesc& desc)
        -> FRhiCommandPoolRef {
        return AdoptResource(new FRhiD3D11CommandPool(desc));
    }

} // namespace AltinaEngine::Rhi
