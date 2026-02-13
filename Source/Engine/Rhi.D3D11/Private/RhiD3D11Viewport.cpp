#include "RhiD3D11/RhiD3D11Viewport.h"

#include "RhiD3D11/RhiD3D11Resources.h"
#include "Rhi/RhiRefs.h"
#include "Logging/Log.h"
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
    #include <d3d11.h>
    #include <dxgi1_6.h>
    #include <wrl/client.h>
#endif

namespace AltinaEngine::Rhi {
#if AE_PLATFORM_WIN
    using Microsoft::WRL::ComPtr;

    struct FRhiD3D11Viewport::FState {
        ComPtr<ID3D11Device>        mDevice;
        ComPtr<ID3D11DeviceContext> mImmediateContext;
        ComPtr<IDXGISwapChain1>     mSwapChain;
        FRhiTextureRef              mBackBuffer;
        u32                         mWidth            = 0U;
        u32                         mHeight           = 0U;
        u32                         mBufferCount      = 2U;
        ERhiFormat                  mFormat           = ERhiFormat::B8G8R8A8Unorm;
        bool                        mAllowTearing     = false;
        bool                        mTearingSupported = false;
    };
#else
    struct FRhiD3D11Viewport::FState {};
#endif

    namespace {
#if AE_PLATFORM_WIN
        auto ToD3D11Format(ERhiFormat format) noexcept -> DXGI_FORMAT {
            switch (format) {
                case ERhiFormat::R8G8B8A8Unorm:
                    return DXGI_FORMAT_R8G8B8A8_UNORM;
                case ERhiFormat::R8G8B8A8UnormSrgb:
                    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                case ERhiFormat::B8G8R8A8Unorm:
                    return DXGI_FORMAT_B8G8R8A8_UNORM;
                case ERhiFormat::B8G8R8A8UnormSrgb:
                    return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
                case ERhiFormat::R16G16B16A16Float:
                    return DXGI_FORMAT_R16G16B16A16_FLOAT;
                case ERhiFormat::R32Float:
                    return DXGI_FORMAT_R32_FLOAT;
                case ERhiFormat::D24UnormS8Uint:
                    return DXGI_FORMAT_D24_UNORM_S8_UINT;
                case ERhiFormat::D32Float:
                    return DXGI_FORMAT_D32_FLOAT;
                case ERhiFormat::Unknown:
                default:
                    return DXGI_FORMAT_UNKNOWN;
            }
        }

        auto QueryTearingSupport(IDXGIFactory2* factory) noexcept -> bool {
            if (factory == nullptr) {
                return false;
            }

            ComPtr<IDXGIFactory5> factory5;
            if (FAILED(factory->QueryInterface(IID_PPV_ARGS(&factory5)))) {
                return false;
            }

            BOOL allowTearing = FALSE;
            if (FAILED(factory5->CheckFeatureSupport(
                    DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)))) {
                return false;
            }

            return allowTearing == TRUE;
        }

        auto ClampExtent(u32 value) noexcept -> u32 { return (value > 0U) ? value : 1U; }
#endif
    } // namespace

    FRhiD3D11Viewport::FRhiD3D11Viewport(
        const FRhiViewportDesc& desc, ID3D11Device* device, ID3D11DeviceContext* immediateContext)
        : FRhiViewport(desc) {
        mState = new FState{};

#if AE_PLATFORM_WIN
        if (!mState) {
            return;
        }

        if (device != nullptr) {
            mState->mDevice = device;
        }
        if (immediateContext != nullptr) {
            mState->mImmediateContext = immediateContext;
        }

        u32 width  = desc.mWidth;
        u32 height = desc.mHeight;
        if ((width == 0U || height == 0U) && desc.mNativeHandle != nullptr) {
            RECT rect{};
            if (GetClientRect(static_cast<HWND>(desc.mNativeHandle), &rect)) {
                const u32 rectWidth  = static_cast<u32>(rect.right - rect.left);
                const u32 rectHeight = static_cast<u32>(rect.bottom - rect.top);
                if (width == 0U) {
                    width = rectWidth;
                }
                if (height == 0U) {
                    height = rectHeight;
                }
            }
        }

        mState->mWidth        = ClampExtent(width);
        mState->mHeight       = ClampExtent(height);
        mState->mBufferCount  = (desc.mBufferCount > 0U) ? desc.mBufferCount : 2U;
        mState->mFormat       = desc.mFormat;
        mState->mAllowTearing = desc.mAllowTearing;

        UpdateExtent(mState->mWidth, mState->mHeight);

        if (!CreateSwapChain() || !CreateBackBuffer()) {
            LogError(TEXT("RHI(D3D11): Failed to create viewport swapchain/backbuffer."));
        }
#else
        (void)device;
        (void)immediateContext;
#endif
    }

    FRhiD3D11Viewport::~FRhiD3D11Viewport() {
        ReleaseBackBuffer();
        delete mState;
        mState = nullptr;
    }

    auto FRhiD3D11Viewport::IsValid() const noexcept -> bool {
#if AE_PLATFORM_WIN
        return mState != nullptr && mState->mSwapChain != nullptr && mState->mBackBuffer;
#else
        return false;
#endif
    }

    auto FRhiD3D11Viewport::GetSwapChain() const noexcept -> IDXGISwapChain* {
#if AE_PLATFORM_WIN
        return (mState != nullptr) ? mState->mSwapChain.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Viewport::GetBackBuffer() const noexcept -> FRhiTexture* {
#if AE_PLATFORM_WIN
        return (mState != nullptr) ? mState->mBackBuffer.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    void FRhiD3D11Viewport::Resize(u32 width, u32 height) {
#if AE_PLATFORM_WIN
        if (!mState || !mState->mSwapChain) {
            return;
        }

        if (width == 0U || height == 0U) {
            return;
        }

        const u32 newWidth  = ClampExtent(width);
        const u32 newHeight = ClampExtent(height);

        if (mState->mWidth == newWidth && mState->mHeight == newHeight) {
            return;
        }

        ReleaseBackBuffer();

        if (mState->mImmediateContext) {
            mState->mImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);
            mState->mImmediateContext->ClearState();
            mState->mImmediateContext->Flush();
        }

        const DXGI_FORMAT format       = ToD3D11Format(mState->mFormat);
        const bool        allowTearing = mState->mAllowTearing && mState->mTearingSupported;
        const UINT        flags        = allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0U;

        HRESULT           hr = mState->mSwapChain->ResizeBuffers(
            mState->mBufferCount, newWidth, newHeight, format, flags);
        if (FAILED(hr)) {
            LogError(TEXT("RHI(D3D11): ResizeBuffers failed (hr=0x{:08X})."), static_cast<u32>(hr));

            if (hr == DXGI_ERROR_INVALID_CALL) {
                mState->mSwapChain.Reset();
                mState->mWidth  = newWidth;
                mState->mHeight = newHeight;
                if (!CreateSwapChain()) {
                    LogError(TEXT("RHI(D3D11): Recreate swapchain failed after ResizeBuffers."));
                    return;
                }

                UpdateExtent(newWidth, newHeight);
                CreateBackBuffer();
                return;
            }

            return;
        }

        mState->mWidth  = newWidth;
        mState->mHeight = newHeight;
        UpdateExtent(newWidth, newHeight);
        CreateBackBuffer();
#else
        (void)width;
        (void)height;
#endif
    }

    void FRhiD3D11Viewport::Present(const FRhiPresentInfo& info) {
#if AE_PLATFORM_WIN
        if (!mState || !mState->mSwapChain) {
            return;
        }

        const u32     syncInterval = info.mSyncInterval;
        const u32     flags        = ResolvePresentFlags(syncInterval, info.mFlags);
        const HRESULT hr           = mState->mSwapChain->Present(syncInterval, flags);
        if (hr == DXGI_STATUS_OCCLUDED) {
            if (mState->mImmediateContext) {
                mState->mImmediateContext->Flush();
            }
        }
#else
        (void)info;
#endif
    }

    bool FRhiD3D11Viewport::CreateSwapChain() {
#if AE_PLATFORM_WIN
        if (!mState || !mState->mDevice) {
            return false;
        }

        const auto& desc = GetDesc();
        if (desc.mNativeHandle == nullptr) {
            LogError(TEXT("RHI(D3D11): Viewport creation failed (no native handle)."));
            return false;
        }

        const DXGI_FORMAT format = ToD3D11Format(mState->mFormat);
        if (format == DXGI_FORMAT_UNKNOWN) {
            LogError(TEXT("RHI(D3D11): Viewport creation failed (unknown format)."));
            return false;
        }

        ComPtr<IDXGIDevice> dxgiDevice;
        if (FAILED(mState->mDevice.As(&dxgiDevice))) {
            return false;
        }

        ComPtr<IDXGIAdapter> adapter;
        if (FAILED(dxgiDevice->GetAdapter(&adapter)) || !adapter) {
            return false;
        }

        ComPtr<IDXGIFactory2> factory;
        if (FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))) || !factory) {
            return false;
        }

        mState->mTearingSupported = QueryTearingSupport(factory.Get());
        mState->mAllowTearing     = mState->mAllowTearing && mState->mTearingSupported;

        DXGI_SWAP_CHAIN_DESC1 swapDesc{};
        swapDesc.Width              = mState->mWidth;
        swapDesc.Height             = mState->mHeight;
        swapDesc.Format             = format;
        swapDesc.Stereo             = FALSE;
        swapDesc.SampleDesc.Count   = 1;
        swapDesc.SampleDesc.Quality = 0;
        swapDesc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDesc.BufferCount        = (mState->mBufferCount < 2U) ? 2U : mState->mBufferCount;
        swapDesc.Scaling            = DXGI_SCALING_STRETCH;
        swapDesc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapDesc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapDesc.Flags = mState->mAllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0U;

        HWND                    hwnd = static_cast<HWND>(desc.mNativeHandle);
        ComPtr<IDXGISwapChain1> swapChain;
        const HRESULT           hr = factory->CreateSwapChainForHwnd(
            mState->mDevice.Get(), hwnd, &swapDesc, nullptr, nullptr, &swapChain);
        if (FAILED(hr) || !swapChain) {
            LogError(TEXT("RHI(D3D11): CreateSwapChainForHwnd failed (hr=0x{:08X})."),
                static_cast<u32>(hr));
            return false;
        }

        factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

        mState->mSwapChain   = AltinaEngine::Move(swapChain);
        mState->mBufferCount = swapDesc.BufferCount;
        return true;
#else
        return false;
#endif
    }

    bool FRhiD3D11Viewport::CreateBackBuffer() {
#if AE_PLATFORM_WIN
        if (!mState || !mState->mSwapChain || !mState->mDevice) {
            return false;
        }

        ReleaseBackBuffer();

        ComPtr<ID3D11Texture2D> backBuffer;
        const HRESULT           hr = mState->mSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        if (FAILED(hr) || !backBuffer) {
            return false;
        }

        ComPtr<ID3D11RenderTargetView> rtv;
        if (FAILED(mState->mDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv))) {
            return false;
        }

        FRhiTextureDesc texDesc{};
        texDesc.mWidth     = mState->mWidth;
        texDesc.mHeight    = mState->mHeight;
        texDesc.mFormat    = mState->mFormat;
        texDesc.mBindFlags = ERhiTextureBindFlags::RenderTarget;
        if (!GetDesc().mDebugName.IsEmptyString()) {
            texDesc.mDebugName = GetDesc().mDebugName;
            texDesc.mDebugName.Append(TEXT(" BackBuffer"));
        }

        auto* texture = new FRhiD3D11Texture(
            texDesc, backBuffer.Detach(), rtv.Detach(), nullptr, nullptr, nullptr);
        mState->mBackBuffer = FRhiTextureRef::Adopt(texture);
        return true;
#else
        return false;
#endif
    }

    void FRhiD3D11Viewport::ReleaseBackBuffer() {
#if AE_PLATFORM_WIN
        if (mState) {
            mState->mBackBuffer.Reset();
        }
#endif
    }

    u32 FRhiD3D11Viewport::ResolvePresentFlags(u32 syncInterval, u32 flags) const noexcept {
#if AE_PLATFORM_WIN
        if (mState) {
            const bool canTear = mState->mAllowTearing && mState->mTearingSupported;
            if (!canTear) {
                flags &= ~DXGI_PRESENT_ALLOW_TEARING;
            }
            if (syncInterval == 0U && canTear) {
                flags |= DXGI_PRESENT_ALLOW_TEARING;
            }
        }
#endif
        return flags;
    }

} // namespace AltinaEngine::Rhi
