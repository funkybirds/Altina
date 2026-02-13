#pragma once

#include "RhiD3D11API.h"
#include "Rhi/RhiShader.h"
#include "Container/SmartPtr.h"

struct ID3D11DeviceChild;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11GeometryShader;
struct ID3D11HullShader;
struct ID3D11DomainShader;
struct ID3D11ComputeShader;

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    class AE_RHI_D3D11_API FRhiD3D11Shader final : public FRhiShader {
    public:
        FRhiD3D11Shader(const FRhiShaderDesc& desc, ID3D11DeviceChild* shader);
        explicit FRhiD3D11Shader(const FRhiShaderDesc& desc);
        ~FRhiD3D11Shader() override;

        FRhiD3D11Shader(const FRhiD3D11Shader&)                                  = delete;
        FRhiD3D11Shader(FRhiD3D11Shader&&)                                       = delete;
        auto               operator=(const FRhiD3D11Shader&) -> FRhiD3D11Shader& = delete;
        auto               operator=(FRhiD3D11Shader&&) -> FRhiD3D11Shader&      = delete;

        [[nodiscard]] auto GetNativeShader() const noexcept -> ID3D11DeviceChild*;
        [[nodiscard]] auto GetVertexShader() const noexcept -> ID3D11VertexShader*;
        [[nodiscard]] auto GetPixelShader() const noexcept -> ID3D11PixelShader*;
        [[nodiscard]] auto GetGeometryShader() const noexcept -> ID3D11GeometryShader*;
        [[nodiscard]] auto GetHullShader() const noexcept -> ID3D11HullShader*;
        [[nodiscard]] auto GetDomainShader() const noexcept -> ID3D11DomainShader*;
        [[nodiscard]] auto GetComputeShader() const noexcept -> ID3D11ComputeShader*;

    private:
        struct FState;
        Container::TOwner<FState> mState;
    };

} // namespace AltinaEngine::Rhi
