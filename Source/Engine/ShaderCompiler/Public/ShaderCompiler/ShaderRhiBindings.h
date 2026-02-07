#pragma once

#include "ShaderCompiler/ShaderCompileTypes.h"
#include "ShaderCompilerAPI.h"

namespace AltinaEngine::ShaderCompiler {
    AE_SHADER_COMPILER_API auto BuildRhiBindingLayout(
        const FShaderReflection& reflection, EShaderStage stage) -> FRhiShaderBindingLayout;
    AE_SHADER_COMPILER_API auto BuildRhiShaderDesc(const FShaderCompileResult& result)
        -> Rhi::FRhiShaderDesc;

} // namespace AltinaEngine::ShaderCompiler
