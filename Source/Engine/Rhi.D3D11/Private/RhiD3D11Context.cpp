#include "RhiD3D11/RhiD3D11Context.h"

#include "RhiD3D11/RhiD3D11Device.h"
#include "Container/SmartPtr.h"
#include "Logging/Log.h"
#include "Types/Aliases.h"
#include "Types/CheckedCast.h"
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
    #include <dxgi1_6.h>
    #include <wrl/client.h>
#endif

#include <type_traits>

namespace AltinaEngine::Rhi {
    using Core::Container::MakeUnique;
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
            using TAllocatorType = Core::Container::TAllocator<TDerived>;
            using Traits         = Core::Container::TAllocatorTraits<TAllocatorType>;

            static_assert(std::is_base_of_v<TBase, TDerived>,
                "MakeSharedAs requires TDerived to derive from TBase.");

            TAllocatorType allocator;
            TDerived*      ptr = Traits::Allocate(allocator, 1);
            try {
                Traits::Construct(allocator, ptr, AltinaEngine::Forward<Args>(args)...);
            } catch (...) {
                Traits::Deallocate(allocator, ptr, 1);
                throw;
            }

            struct FDeleter {
                TAllocatorType mAllocator;
                void           operator()(TBase* basePtr) {
                    if (!basePtr) {
                        return;
                    }
                    auto* derivedPtr = AltinaEngine::CheckedCast<TDerived*>(basePtr);
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
            if (name == nullptr) {
                return;
            }

            const int required =
                WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr, nullptr);
            if (required <= 0) {
                return;
            }

            Core::Container::TVector<char> buffer;
            buffer.Resize(static_cast<usize>(required));
            const int written = WideCharToMultiByte(
                CP_UTF8, 0, name, -1, buffer.Data(), required, nullptr, nullptr);
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

            const D3D_FEATURE_LEVEL levelsWith11_1[] = { D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };

            const D3D_FEATURE_LEVEL levelsFallback[] = { D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };

            ID3D11Device*           device  = nullptr;
            ID3D11DeviceContext*    context = nullptr;
            const D3D_DRIVER_TYPE   driverType =
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
                if (device != nullptr) {
                    device->Release();
                }
                if (context != nullptr) {
                    context->Release();
                }
            }

            return hr;
        }

        class FRhiD3D11Adapter final : public FRhiAdapter {
        public:
            FRhiD3D11Adapter(const FRhiAdapterDesc& desc, ComPtr<IDXGIAdapter1> adapter)
                : FRhiAdapter(desc), mAdapter(AltinaEngine::Move(adapter)) {}

            [[nodiscard]] auto GetAdapter() const noexcept -> IDXGIAdapter1* {
                return mAdapter.Get();
            }

        private:
            ComPtr<IDXGIAdapter1> mAdapter;
        };

#else
        class FRhiD3D11Adapter final : public FRhiAdapter {
        public:
            explicit FRhiD3D11Adapter(const FRhiAdapterDesc& desc) : FRhiAdapter(desc) {}
        };
#endif
    } // namespace

    FRhiD3D11Context::FRhiD3D11Context() {
#if AE_PLATFORM_WIN
        mState = MakeUnique<FRhiD3D11ContextState>();
#endif
    }

    FRhiD3D11Context::~FRhiD3D11Context() {
        Shutdown();
        mState.reset();
    }

    auto FRhiD3D11Context::InitializeBackend(const FRhiInitDesc& desc) -> bool {
#if AE_PLATFORM_WIN
        if (!mState) {
            mState = MakeUnique<FRhiD3D11ContextState>();
        }
        LogInfo(TEXT("RHI(D3D11): Initializing (DebugLayer={}, GPUValidation={})."),
            desc.mEnableDebugLayer, desc.mEnableGpuValidation);

        const bool ok = CreateFactory(*mState, desc);
        if (!ok) {
            LogError(TEXT("RHI(D3D11): Failed to create DXGI factory."));
        } else {
            LogInfo(TEXT("RHI(D3D11): DXGI factory created."));
        }
        return ok;
#else
        (void)desc;
        LogWarning(TEXT("RHI(D3D11): Initialization requested on non-Windows platform."));
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

    void FRhiD3D11Context::EnumerateAdaptersInternal(TVector<TShared<FRhiAdapter>>& outAdapters) {
        outAdapters.Clear();

#if AE_PLATFORM_WIN
        if (!mState || ((mState->mFactory6 == nullptr) && (mState->mFactory1 == nullptr))) {
            return;
        }

        for (UINT index = 0;; ++index) {
            ComPtr<IDXGIAdapter1> adapter;
            HRESULT               hr = E_FAIL;

            if (mState->mFactory6 != nullptr) {
                hr = mState->mFactory6->EnumAdapterByGpuPreference(
                    index, DXGI_GPU_PREFERENCE_UNSPECIFIED, IID_PPV_ARGS(&adapter));
            } else if (mState->mFactory1 != nullptr) {
                hr = mState->mFactory1->EnumAdapters1(index, &adapter);
            }

            if (hr == DXGI_ERROR_NOT_FOUND) {
                break;
            }

            if (FAILED(hr) || (adapter == nullptr)) {
                continue;
            }

            DXGI_ADAPTER_DESC1 dxgiDesc = {};
            if (FAILED(adapter->GetDesc1(&dxgiDesc))) {
                continue;
            }

            FRhiAdapterDesc desc;
            AssignAdapterName(desc, dxgiDesc.Description);
            desc.mVendorId                   = ToVendorId(dxgiDesc.VendorId);
            desc.mDeviceId                   = dxgiDesc.DeviceId;
            desc.mType                       = ToAdapterType(dxgiDesc);
            desc.mDedicatedVideoMemoryBytes  = dxgiDesc.DedicatedVideoMemory;
            desc.mDedicatedSystemMemoryBytes = dxgiDesc.DedicatedSystemMemory;
            desc.mSharedSystemMemoryBytes    = dxgiDesc.SharedSystemMemory;

            outAdapters.PushBack(MakeSharedAs<FRhiAdapter, FRhiD3D11Adapter>(desc, adapter));
        }
#endif
    }

    auto FRhiD3D11Context::CreateDeviceInternal(
        const TShared<FRhiAdapter>& adapter, const FRhiDeviceDesc& desc) -> TShared<FRhiDevice> {
        if (!adapter) {
            return {};
        }

#if AE_PLATFORM_WIN
        const auto* d3dAdapter = AltinaEngine::CheckedCast<const FRhiD3D11Adapter*>(adapter.Get());
        IDXGIAdapter1*       nativeAdapter = (d3dAdapter != nullptr) ? d3dAdapter->GetAdapter() : nullptr;

        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        D3D_FEATURE_LEVEL           featureLevel = D3D_FEATURE_LEVEL_11_0;

        const bool wantsDebug = desc.mEnableDebugLayer || desc.mEnableGpuValidation;

        HRESULT hr = TryCreateD3D11Device(nativeAdapter, wantsDebug, device, context, featureLevel);
        if (FAILED(hr) && wantsDebug) {
            hr = TryCreateD3D11Device(nativeAdapter, false, device, context, featureLevel);
        }

        if (FAILED(hr)) {
            return {};
        }

        return MakeSharedAs<FRhiDevice, FRhiD3D11Device>(desc, adapter->GetDesc(), device.Detach(),
            context.Detach(), static_cast<u32>(featureLevel));
#else
        (void)desc;
        return MakeSharedAs<FRhiDevice, FRhiD3D11Device>(
            desc, adapter->GetDesc(), nullptr, nullptr, 0U);
#endif
    }

} // namespace AltinaEngine::Rhi
