#pragma once

#include "Rendering/RenderingAPI.h"

#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Asset/MaterialAsset.h"
#include "Asset/MeshMaterialParameterBlock.h"
#include "Container/SmartPtr.h"
#include "Container/StringView.h"
#include "Material/Material.h"
#include "Material/MaterialTemplate.h"
#include "Shader/ShaderRegistry.h"
#include "Shader/ShaderTypes.h"
namespace AltinaEngine::ShaderCompiler {
    struct FShaderCompileResult;
} // namespace AltinaEngine::ShaderCompiler

namespace AltinaEngine::Rendering {
    [[nodiscard]] auto AE_RENDERING_API CompileShaderFromAsset(const Asset::FAssetHandle& handle,
        Core::Container::FStringView entry, Shader::EShaderStage stage,
        Asset::FAssetRegistry& registry, Asset::FAssetManager& manager,
        RenderCore::FShaderRegistry::FShaderKey& outKey,
        ShaderCompiler::FShaderCompileResult&    outResult) -> bool;

    [[nodiscard]] auto AE_RENDERING_API BuildMaterialTemplateFromAsset(
        const Asset::FMaterialAsset& asset, Asset::FAssetRegistry& registry,
        Asset::FAssetManager& manager) -> Core::Container::TShared<RenderCore::FMaterialTemplate>;

    [[nodiscard]] auto AE_RENDERING_API BuildRenderMaterialFromAsset(
        const Asset::FAssetHandle& handle, const Asset::FMeshMaterialParameterBlock& parameters,
        Asset::FAssetRegistry& registry, Asset::FAssetManager& manager) -> RenderCore::FMaterial;
} // namespace AltinaEngine::Rendering
