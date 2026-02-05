#include "RhiD3D11/RhiD3D11Context.h"

#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiCommandPool.h"
#include "Rhi/RhiFence.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiSemaphore.h"
#include "Rhi/RhiShader.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiTexture.h"
#include "Container/SmartPtr.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"
#include <type_traits>

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
    #include <dxgi1_6.h>
    #include <wrl/client.h>
#endif

namespace AltinaEngine::Rhi {
#if AE_PLATFORM_WIN
    using Microsoft::WRL::ComPtr;

    struct FRhiD3D11ContextState {
        ComPtr<IDXGIFactory6> mFactory6;
        ComPtr<IDXGIFactory1> mFactory1;
    };
#else
    struct FRhiD3D11ContextState {};
#endif

    namespace {
        template <typename TBase, typename TDerived, typename... Args>
        auto MakeSharedAs(Args&&... args) -> TShared<TBase> {
            using AllocatorType = Core::Container::TAllocator<TDerived>;
            using Traits        = Core::Container::TAllocatorTraits<AllocatorType>;

            static_assert(std::is_base_of_v<TBase, TDerived>,
                "MakeSharedAs requires TDerived to derive from TBase.");

            AllocatorType allocator;
            TDerived*     ptr = Traits::Allocate(allocator, 1);
            try {
                Traits::Construct(allocator, ptr, AltinaEngine::Forward<Args>(args)...);
            } catch (...) {
                Traits::Deallocate(allocator, ptr, 1);
                throw;
            }

            struct FDeleter {
                AllocatorType mAllocator;
                void          operator()(TBase* basePtr) {
                    if (!basePtr) {
                        return;
                    }
                    auto* derivedPtr = static_cast<TDerived*>(basePtr);
                    Traits::Destroy(mAllocator, derivedPtr);
                    Traits::Deallocate(mAllocator, derivedPtr, 1);
                }
            };

            return TShared<TBase>(ptr, FDeleter{ allocator });
        }

#if AE_PLATFORM_WIN
        auto ToVendorId(u32 vendorId) noexcept -> ERhiVendorId {
            switch (vendorId) {
            case static_cast<u32>(ERhiVendorId::Nvidia):
                return ERhiVendorId::Nvidia;
            case static_cast<u32>(ERhiVendorId::AMD):
                return ERhiVendorId::AMD;
            case static_cast<u32>(ERhiVendorId::Intel):
                return ERhiVendorId::Intel;
            case static_cast<u32>(ERhiVendorId::Microsoft):
                return ERhiVendorId::Microsoft;
            default:
                return ERhiVendorId::Unknown;
            }
        }

        auto ToAdapterType(const DXGI_ADAPTER_DESC1& desc) noexcept -> ERhiAdapterType {
            if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0U) {
                return ERhiAdapterType::Software;
            }

            if (desc.DedicatedVideoMemory == 0ULL) {
                return ERhiAdapterType::Integrated;
            }

            return ERhiAdapterType::Discrete;
        }

        void AssignAdapterName(FRhiAdapterDesc& outDesc, const wchar_t* name) {
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
            outDesc.mName.Assign(name);
#else
            if (!name) {
                return;
            }

            const int required =
                WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr, nullptr);
            if (required <= 0) {
                return;
            }

            Core::Container::TVector<char> buffer;
            buffer.Resize(static_cast<usize>(required));
            const int written =
                WideCharToMultiByte(CP_UTF8, 0, name, -1, buffer.Data(), required, nullptr, nullptr);
            if (written > 0) {
                outDesc.mName.Assign(buffer.Data());
            }
#endif
        }

        auto CreateFactory(FRhiD3D11ContextState& state, const FRhiInitDesc& desc) -> bool {
            state.mFactory6.Reset();
            state.mFactory1.Reset();

            UINT flags = 0U;
            if (desc.mEnableDebugLayer) {
                flags |= DXGI_CREATE_FACTORY_DEBUG;
            }

            HRESULT hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(&state.mFactory6));
            if (FAILED(hr)) {
                hr = CreateDXGIFactory1(IID_PPV_ARGS(&state.mFactory1));
                return SUCCEEDED(hr);
            }

            state.mFactory6.As(&state.mFactory1);
            return true;
        }

        auto TryCreateD3D11Device(IDXGIAdapter1* adapter, bool enableDebug,
            ComPtr<ID3D11Device>& outDevice, ComPtr<ID3D11DeviceContext>& outContext,
            D3D_FEATURE_LEVEL& outFeatureLevel) -> HRESULT {
            UINT flags = 0U;
            if (enableDebug) {
                flags |= D3D11_CREATE_DEVICE_DEBUG;
            }

            const D3D_FEATURE_LEVEL levelsWith11_1[] = {
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_10_1,
                D3D_FEATURE_LEVEL_10_0
            };

            const D3D_FEATURE_LEVEL levelsFallback[] = {
                D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_10_1,
                D3D_FEATURE_LEVEL_10_0
            };

            ID3D11Device*        device  = nullptr;
            ID3D11DeviceContext* context = nullptr;
            const D3D_DRIVER_TYPE driverType =
                (adapter != nullptr) ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;

            HRESULT hr = D3D11CreateDevice(adapter, driverType, nullptr, flags, levelsWith11_1,
                static_cast<UINT>(_countof(levelsWith11_1)), D3D11_SDK_VERSION, &device,
                &outFeatureLevel, &context);

            if (hr == E_INVALIDARG) {
                hr = D3D11CreateDevice(adapter, driverType, nullptr, flags, levelsFallback,
                    static_cast<UINT>(_countof(levelsFallback)), D3D11_SDK_VERSION, &device,
                    &outFeatureLevel, &context);
            }

            if (SUCCEEDED(hr)) {
                outDevice.Attach(device);
                outContext.Attach(context);
            } else {
                if (device) {
                    device->Release();
                }
                if (context) {
                    context->Release();
                }
            }

            return hr;
        }

        class FRhiD3D11Adapter final : public FRhiAdapter {
        public:
            FRhiD3D11Adapter(const FRhiAdapterDesc& desc, ComPtr<IDXGIAdapter1> adapter)
                : FRhiAdapter(desc), mAdapter(AltinaEngine::Move(adapter)) {}

            auto GetAdapter() const noexcept -> IDXGIAdapter1* { return mAdapter.Get(); }

        private:
            ComPtr<IDXGIAdapter1> mAdapter;
        };

        class FRhiD3D11Buffer final : public FRhiBuffer {
        public:
            explicit FRhiD3D11Buffer(const FRhiBufferDesc& desc) : FRhiBuffer(desc) {}
        };

        class FRhiD3D11Texture final : public FRhiTexture {
        public:
            explicit FRhiD3D11Texture(const FRhiTextureDesc& desc) : FRhiTexture(desc) {}
        };

        class FRhiD3D11Sampler final : public FRhiSampler {
        public:
            explicit FRhiD3D11Sampler(const FRhiSamplerDesc& desc) : FRhiSampler(desc) {}
        };

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

        class FRhiD3D11Device final : public FRhiDevice {
        public:
            FRhiD3D11Device(const FRhiDeviceDesc& desc, const FRhiAdapterDesc& adapterDesc,
                ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context,
                D3D_FEATURE_LEVEL featureLevel)
                : FRhiDevice(desc, adapterDesc)
                , mDevice(AltinaEngine::Move(device))
                , mImmediateContext(AltinaEngine::Move(context))
                , mFeatureLevel(featureLevel) {
                FRhiSupportedLimits limits;
                limits.mMaxTextureDimension1D     = D3D11_REQ_TEXTURE1D_U_DIMENSION;
                limits.mMaxTextureDimension2D     = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
                limits.mMaxTextureDimension3D     = D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
                limits.mMaxTextureArrayLayers     = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
                limits.mMaxSamplers               = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
                limits.mMaxColorAttachments       = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
                SetSupportedLimits(limits);

                RegisterQueue(ERhiQueueType::Graphics,
                    AdoptResource(new FRhiD3D11Queue(ERhiQueueType::Graphics)));
                RegisterQueue(ERhiQueueType::Compute,
                    AdoptResource(new FRhiD3D11Queue(ERhiQueueType::Compute)));
                RegisterQueue(ERhiQueueType::Copy,
                    AdoptResource(new FRhiD3D11Queue(ERhiQueueType::Copy)));
            }

            auto CreateBuffer(const FRhiBufferDesc& desc) -> FRhiBufferRef override {
                return AdoptResource(new FRhiD3D11Buffer(desc));
            }

            auto CreateTexture(const FRhiTextureDesc& desc) -> FRhiTextureRef override {
                return AdoptResource(new FRhiD3D11Texture(desc));
            }

            auto CreateSampler(const FRhiSamplerDesc& desc) -> FRhiSamplerRef override {
                return AdoptResource(new FRhiD3D11Sampler(desc));
            }

            auto CreateShader(const FRhiShaderDesc& desc) -> FRhiShaderRef override {
                return AdoptResource(new FRhiD3D11Shader(desc));
            }

            auto CreateGraphicsPipeline(const FRhiGraphicsPipelineDesc& desc)
                -> FRhiPipelineRef override {
                return AdoptResource(new FRhiD3D11GraphicsPipeline(desc));
            }

            auto CreateComputePipeline(const FRhiComputePipelineDesc& desc)
                -> FRhiPipelineRef override {
                return AdoptResource(new FRhiD3D11ComputePipeline(desc));
            }

            auto CreatePipelineLayout(const FRhiPipelineLayoutDesc& desc)
                -> FRhiPipelineLayoutRef override {
                return AdoptResource(new FRhiD3D11PipelineLayout(desc));
            }

            auto CreateBindGroupLayout(const FRhiBindGroupLayoutDesc& desc)
                -> FRhiBindGroupLayoutRef override {
                return AdoptResource(new FRhiD3D11BindGroupLayout(desc));
            }

            auto CreateBindGroup(const FRhiBindGroupDesc& desc) -> FRhiBindGroupRef override {
                return AdoptResource(new FRhiD3D11BindGroup(desc));
            }

            auto CreateFence(bool /*signaled*/) -> FRhiFenceRef override {
                return AdoptResource(new FRhiD3D11Fence());
            }

            auto CreateSemaphore() -> FRhiSemaphoreRef override {
                return AdoptResource(new FRhiD3D11Semaphore());
            }

            auto CreateCommandPool(const FRhiCommandPoolDesc& desc)
                -> FRhiCommandPoolRef override {
                return AdoptResource(new FRhiD3D11CommandPool(desc));
            }

        private:
            ComPtr<ID3D11Device>        mDevice;
            ComPtr<ID3D11DeviceContext> mImmediateContext;
            D3D_FEATURE_LEVEL           mFeatureLevel = D3D_FEATURE_LEVEL_11_0;
        };
#else
        class FRhiD3D11Adapter final : public FRhiAdapter {
        public:
            explicit FRhiD3D11Adapter(const FRhiAdapterDesc& desc) : FRhiAdapter(desc) {}
        };

        class FRhiD3D11Buffer final : public FRhiBuffer {
        public:
            explicit FRhiD3D11Buffer(const FRhiBufferDesc& desc) : FRhiBuffer(desc) {}
        };

        class FRhiD3D11Texture final : public FRhiTexture {
        public:
            explicit FRhiD3D11Texture(const FRhiTextureDesc& desc) : FRhiTexture(desc) {}
        };

        class FRhiD3D11Sampler final : public FRhiSampler {
        public:
            explicit FRhiD3D11Sampler(const FRhiSamplerDesc& desc) : FRhiSampler(desc) {}
        };

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

        class FRhiD3D11Device final : public FRhiDevice {
        public:
            FRhiD3D11Device(const FRhiDeviceDesc& desc, const FRhiAdapterDesc& adapterDesc)
                : FRhiDevice(desc, adapterDesc) {
                RegisterQueue(ERhiQueueType::Graphics,
                    AdoptResource(new FRhiD3D11Queue(ERhiQueueType::Graphics)));
                RegisterQueue(ERhiQueueType::Compute,
                    AdoptResource(new FRhiD3D11Queue(ERhiQueueType::Compute)));
                RegisterQueue(ERhiQueueType::Copy,
                    AdoptResource(new FRhiD3D11Queue(ERhiQueueType::Copy)));
            }

            auto CreateBuffer(const FRhiBufferDesc& desc) -> FRhiBufferRef override {
                return AdoptResource(new FRhiD3D11Buffer(desc));
            }

            auto CreateTexture(const FRhiTextureDesc& desc) -> FRhiTextureRef override {
                return AdoptResource(new FRhiD3D11Texture(desc));
            }

            auto CreateSampler(const FRhiSamplerDesc& desc) -> FRhiSamplerRef override {
                return AdoptResource(new FRhiD3D11Sampler(desc));
            }

            auto CreateShader(const FRhiShaderDesc& desc) -> FRhiShaderRef override {
                return AdoptResource(new FRhiD3D11Shader(desc));
            }

            auto CreateGraphicsPipeline(const FRhiGraphicsPipelineDesc& desc)
                -> FRhiPipelineRef override {
                return AdoptResource(new FRhiD3D11GraphicsPipeline(desc));
            }

            auto CreateComputePipeline(const FRhiComputePipelineDesc& desc)
                -> FRhiPipelineRef override {
                return AdoptResource(new FRhiD3D11ComputePipeline(desc));
            }

            auto CreatePipelineLayout(const FRhiPipelineLayoutDesc& desc)
                -> FRhiPipelineLayoutRef override {
                return AdoptResource(new FRhiD3D11PipelineLayout(desc));
            }

            auto CreateBindGroupLayout(const FRhiBindGroupLayoutDesc& desc)
                -> FRhiBindGroupLayoutRef override {
                return AdoptResource(new FRhiD3D11BindGroupLayout(desc));
            }

            auto CreateBindGroup(const FRhiBindGroupDesc& desc) -> FRhiBindGroupRef override {
                return AdoptResource(new FRhiD3D11BindGroup(desc));
            }

            auto CreateFence(bool /*signaled*/) -> FRhiFenceRef override {
                return AdoptResource(new FRhiD3D11Fence());
            }

            auto CreateSemaphore() -> FRhiSemaphoreRef override {
                return AdoptResource(new FRhiD3D11Semaphore());
            }

            auto CreateCommandPool(const FRhiCommandPoolDesc& desc)
                -> FRhiCommandPoolRef override {
                return AdoptResource(new FRhiD3D11CommandPool(desc));
            }
        };
#endif
    } // namespace

    FRhiD3D11Context::FRhiD3D11Context() {
#if AE_PLATFORM_WIN
        mState = new FRhiD3D11ContextState{};
#endif
    }

    FRhiD3D11Context::~FRhiD3D11Context() {
        Shutdown();
        delete mState;
        mState = nullptr;
    }

    auto FRhiD3D11Context::InitializeBackend(const FRhiInitDesc& desc) -> bool {
#if AE_PLATFORM_WIN
        if (!mState) {
            mState = new FRhiD3D11ContextState{};
        }
        return CreateFactory(*mState, desc);
#else
        (void)desc;
        return false;
#endif
    }

    void FRhiD3D11Context::ShutdownBackend() {
#if AE_PLATFORM_WIN
        if (!mState) {
            return;
        }

        mState->mFactory6.Reset();
        mState->mFactory1.Reset();
#endif
    }

    void FRhiD3D11Context::EnumerateAdaptersInternal(
        TVector<TShared<FRhiAdapter>>& outAdapters) {
        outAdapters.Clear();

#if AE_PLATFORM_WIN
        if (!mState || (!mState->mFactory6 && !mState->mFactory1)) {
            return;
        }

        for (UINT index = 0; ; ++index) {
            ComPtr<IDXGIAdapter1> adapter;
            HRESULT               hr = E_FAIL;

            if (mState->mFactory6) {
                hr = mState->mFactory6->EnumAdapterByGpuPreference(index,
                    DXGI_GPU_PREFERENCE_UNSPECIFIED, IID_PPV_ARGS(&adapter));
            } else if (mState->mFactory1) {
                hr = mState->mFactory1->EnumAdapters1(index, &adapter);
            }

            if (hr == DXGI_ERROR_NOT_FOUND) {
                break;
            }

            if (FAILED(hr) || !adapter) {
                continue;
            }

            DXGI_ADAPTER_DESC1 dxgiDesc = {};
            if (FAILED(adapter->GetDesc1(&dxgiDesc))) {
                continue;
            }

            FRhiAdapterDesc desc;
            AssignAdapterName(desc, dxgiDesc.Description);
            desc.mVendorId                  = ToVendorId(dxgiDesc.VendorId);
            desc.mDeviceId                  = dxgiDesc.DeviceId;
            desc.mType                      = ToAdapterType(dxgiDesc);
            desc.mDedicatedVideoMemoryBytes = dxgiDesc.DedicatedVideoMemory;
            desc.mDedicatedSystemMemoryBytes = dxgiDesc.DedicatedSystemMemory;
            desc.mSharedSystemMemoryBytes   = dxgiDesc.SharedSystemMemory;

            outAdapters.PushBack(
                MakeSharedAs<FRhiAdapter, FRhiD3D11Adapter>(desc, adapter));
        }
#endif
    }

    auto FRhiD3D11Context::CreateDeviceInternal(
        const TShared<FRhiAdapter>& adapter, const FRhiDeviceDesc& desc)
        -> TShared<FRhiDevice> {
        if (!adapter) {
            return {};
        }

#if AE_PLATFORM_WIN
        const auto* d3dAdapter = static_cast<const FRhiD3D11Adapter*>(adapter.Get());
        IDXGIAdapter1* nativeAdapter =
            d3dAdapter ? d3dAdapter->GetAdapter() : nullptr;

        ComPtr<ID3D11Device>        device;
        ComPtr<ID3D11DeviceContext> context;
        D3D_FEATURE_LEVEL           featureLevel = D3D_FEATURE_LEVEL_11_0;

        const bool wantsDebug =
            desc.mEnableDebugLayer || desc.mEnableGpuValidation;

        HRESULT hr = TryCreateD3D11Device(nativeAdapter, wantsDebug, device, context,
            featureLevel);
        if (FAILED(hr) && wantsDebug) {
            hr = TryCreateD3D11Device(nativeAdapter, false, device, context, featureLevel);
        }

        if (FAILED(hr)) {
            return {};
        }

        return MakeSharedAs<FRhiDevice, FRhiD3D11Device>(
            desc, adapter->GetDesc(), AltinaEngine::Move(device),
            AltinaEngine::Move(context), featureLevel);
#else
        (void)desc;
        return MakeSharedAs<FRhiDevice, FRhiD3D11Device>(desc, adapter->GetDesc());
#endif
    }

} // namespace AltinaEngine::Rhi
