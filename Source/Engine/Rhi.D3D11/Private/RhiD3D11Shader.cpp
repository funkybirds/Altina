#include "RhiD3D11/RhiD3D11Shader.h"

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
    #include <wrl/client.h>
#endif

namespace AltinaEngine::Rhi {
#if AE_PLATFORM_WIN
    using Microsoft::WRL::ComPtr;

    struct FRhiD3D11Shader::FState {
        ComPtr<ID3D11DeviceChild> mShader;
    };
#else
    struct FRhiD3D11Shader::FState {};
#endif

    FRhiD3D11Shader::FRhiD3D11Shader(const FRhiShaderDesc& desc, ID3D11DeviceChild* shader)
        : FRhiShader(desc) {
#if AE_PLATFORM_WIN
        mState = new FState{};
        if (mState && shader) {
            mState->mShader.Attach(shader);
        }
#else
        (void)shader;
#endif
    }

    FRhiD3D11Shader::FRhiD3D11Shader(const FRhiShaderDesc& desc) : FRhiShader(desc) {
#if AE_PLATFORM_WIN
        mState = new FState{};
#endif
    }

    FRhiD3D11Shader::~FRhiD3D11Shader() {
#if AE_PLATFORM_WIN
        delete mState;
        mState = nullptr;
#endif
    }

    auto FRhiD3D11Shader::GetNativeShader() const noexcept -> ID3D11DeviceChild* {
#if AE_PLATFORM_WIN
        return mState ? mState->mShader.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Shader::GetVertexShader() const noexcept -> ID3D11VertexShader* {
#if AE_PLATFORM_WIN
        return (GetDesc().mStage == EShaderStage::Vertex)
            ? static_cast<ID3D11VertexShader*>(GetNativeShader())
            : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Shader::GetPixelShader() const noexcept -> ID3D11PixelShader* {
#if AE_PLATFORM_WIN
        return (GetDesc().mStage == EShaderStage::Pixel)
            ? static_cast<ID3D11PixelShader*>(GetNativeShader())
            : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Shader::GetGeometryShader() const noexcept -> ID3D11GeometryShader* {
#if AE_PLATFORM_WIN
        return (GetDesc().mStage == EShaderStage::Geometry)
            ? static_cast<ID3D11GeometryShader*>(GetNativeShader())
            : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Shader::GetHullShader() const noexcept -> ID3D11HullShader* {
#if AE_PLATFORM_WIN
        return (GetDesc().mStage == EShaderStage::Hull)
            ? static_cast<ID3D11HullShader*>(GetNativeShader())
            : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Shader::GetDomainShader() const noexcept -> ID3D11DomainShader* {
#if AE_PLATFORM_WIN
        return (GetDesc().mStage == EShaderStage::Domain)
            ? static_cast<ID3D11DomainShader*>(GetNativeShader())
            : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Shader::GetComputeShader() const noexcept -> ID3D11ComputeShader* {
#if AE_PLATFORM_WIN
        return (GetDesc().mStage == EShaderStage::Compute)
            ? static_cast<ID3D11ComputeShader*>(GetNativeShader())
            : nullptr;
#else
        return nullptr;
#endif
    }

} // namespace AltinaEngine::Rhi
