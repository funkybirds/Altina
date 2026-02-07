#pragma once

#include "ShaderCompiler/ShaderCompileTypes.h"
#include "ShaderCompilerAPI.h"
#include "Container/Function.h"

namespace AltinaEngine::ShaderCompiler {
    using Core::Container::TFunction;

    using FOnShaderCompiled = TFunction<void(const FShaderCompileResult&)>;

    class AE_SHADER_COMPILER_API IShaderCompiler {
    public:
        virtual ~IShaderCompiler() = default;

        virtual auto Compile(const FShaderCompileRequest& request) -> FShaderCompileResult = 0;

        virtual void CompileAsync(const FShaderCompileRequest& request,
            FOnShaderCompiled onCompleted) = 0;
    };

    AE_SHADER_COMPILER_API auto GetShaderCompiler() -> IShaderCompiler&;

} // namespace AltinaEngine::ShaderCompiler
