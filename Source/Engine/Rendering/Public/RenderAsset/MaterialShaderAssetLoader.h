#pragma once

#include "Rendering/RenderingAPI.h"

#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Asset/MaterialAsset.h"
#include "Container/SmartPtr.h"
#include "Container/StringView.h"
#include "Material/MaterialTemplate.h"
#include "Shader/ShaderRegistry.h"
#include "Shader/ShaderTypes.h"
#include "ShaderCompiler/ShaderCompiler.h"

namespace AltinaEngine::Rendering {
    [[nodiscard]] auto AE_RENDERING_API CompileShaderFromAsset(const Asset::FAssetHandle& handle,
        Core::Container::FStringView entry, Shader::EShaderStage stage,
        Asset::FAssetRegistry& registry, Asset::FAssetManager& manager,
        RenderCore::FShaderRegistry::FShaderKey& outKey,
        ShaderCompiler::FShaderCompileResult& outResult) -> bool;

    [[nodiscard]] auto AE_RENDERING_API BuildMaterialTemplateFromAsset(
        const Asset::FMaterialAsset& asset, Asset::FAssetRegistry& registry,
        Asset::FAssetManager& manager) -> Core::Container::TShared<RenderCore::FMaterialTemplate>;
} // namespace AltinaEngine::Rendering
