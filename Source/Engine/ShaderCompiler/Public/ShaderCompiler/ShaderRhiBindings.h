#pragma once

#include "ShaderCompiler/ShaderCompileTypes.h"
#include "ShaderCompilerAPI.h"

namespace AltinaEngine::ShaderCompiler {
    AE_SHADER_COMPILER_API auto BuildRhiBindingLayout(
        const FShaderReflection& reflection, EShaderStage stage) -> FRhiShaderBindingLayout;

} // namespace AltinaEngine::ShaderCompiler
