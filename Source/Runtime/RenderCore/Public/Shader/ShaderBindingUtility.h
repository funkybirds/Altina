#pragma once

#include "RenderCoreAPI.h"

#include "Container/StringView.h"
#include "Container/Vector.h"
#include "Rhi/RhiEnums.h"
#include "Rhi/RhiStructs.h"
#include "Shader/ShaderReflection.h"
#include "Shader/ShaderRegistry.h"
#include "Shader/ShaderTypes.h"

namespace AltinaEngine::RenderCore::ShaderBinding {
    namespace Container = Core::Container;
    using Container::FStringView;
    using Container::TVector;

    AE_RENDER_CORE_API auto ToRhiShaderStageFlags(Shader::EShaderStage stage) noexcept
        -> Rhi::ERhiShaderStageFlags;

    AE_RENDER_CORE_API auto OrShaderStageFlags(Rhi::ERhiShaderStageFlags lhs,
        Rhi::ERhiShaderStageFlags rhs) noexcept -> Rhi::ERhiShaderStageFlags;

    AE_RENDER_CORE_API auto ToRhiBindingType(
        const Shader::FShaderResourceBinding& resource) noexcept -> Rhi::ERhiBindingType;

    AE_RENDER_CORE_API auto BuildBindGroupLayoutFromShaderSet(
        const RenderCore::FShaderRegistry&                      registry,
        const TVector<RenderCore::FShaderRegistry::FShaderKey>& shaderKeys, u32 setIndex,
        Rhi::FRhiBindGroupLayoutDesc& outLayoutDesc) -> bool;

    AE_RENDER_CORE_API auto ResolveConstantBufferBindingByName(
        const RenderCore::FShaderRegistry&                      registry,
        const TVector<RenderCore::FShaderRegistry::FShaderKey>& shaderKeys, FStringView cbufferName,
        u32& outSetIndex, u32& outBinding, Rhi::ERhiShaderStageFlags& outVisibility) -> bool;
} // namespace AltinaEngine::RenderCore::ShaderBinding
