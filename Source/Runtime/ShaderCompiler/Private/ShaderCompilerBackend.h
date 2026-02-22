#pragma once

#include "ShaderCompiler/ShaderCompileTypes.h"
#include "Container/StringView.h"
#include "Types/Aliases.h"

namespace AltinaEngine::ShaderCompiler::Detail {
    namespace Container = Core::Container;
    using Container::FStringView;

#ifndef AE_SHADER_COMPILER_ENABLE_DXC
    #define AE_SHADER_COMPILER_ENABLE_DXC 0
#endif

#ifndef AE_SHADER_COMPILER_ENABLE_SLANG
    #define AE_SHADER_COMPILER_ENABLE_SLANG 0
#endif

    class IShaderCompilerBackend {
    public:
        virtual ~IShaderCompilerBackend() = default;

        [[nodiscard]] virtual auto GetDisplayName() const noexcept -> FStringView          = 0;
        [[nodiscard]] virtual auto IsAvailable() const noexcept -> bool                    = 0;
        virtual auto Compile(const FShaderCompileRequest& request) -> FShaderCompileResult = 0;
    };

    class FDxcCompilerBackend final : public IShaderCompilerBackend {
    public:
        [[nodiscard]] auto GetDisplayName() const noexcept -> FStringView override;
        [[nodiscard]] auto IsAvailable() const noexcept -> bool override;
        auto Compile(const FShaderCompileRequest& request) -> FShaderCompileResult override;
    };

    class FSlangCompilerBackend final : public IShaderCompilerBackend {
    public:
        [[nodiscard]] auto GetDisplayName() const noexcept -> FStringView override;
        [[nodiscard]] auto IsAvailable() const noexcept -> bool override;
        auto Compile(const FShaderCompileRequest& request) -> FShaderCompileResult override;
    };

} // namespace AltinaEngine::ShaderCompiler::Detail
