#include "RenderAsset/MaterialShaderAssetLoader.h"

#include "Algorithm/CStringUtils.h"
#include "Asset/ShaderAsset.h"
#include "Container/String.h"
#include "Logging/Log.h"
#include "Material/Material.h"
#include "Asset/AssetBinary.h"
#include "Asset/Texture2DAsset.h"
#include "Rhi/RhiInit.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Rendering/BasicDeferredRenderer.h"
#include "Shader/ShaderPermutation.h"
#include "Shader/ShaderReflection.h"
#include "ShaderCompiler/ShaderPermutationParser.h"
#include "ShaderCompiler/ShaderCompiler.h"
#include "ShaderCompiler/ShaderRhiBindings.h"
#include "Types/Traits.h"
#include "Utility/String/CodeConvert.h"
#include "Utility/Filesystem/Path.h"
#include "Utility/Filesystem/PathUtils.h"

#include <fstream>

using namespace AltinaEngine;

namespace AltinaEngine::Rendering {
    namespace {
        namespace Container = Core::Container;
        using Container::FString;
        using Container::FStringView;

        auto TryParseMaterialPass(FStringView name, RenderCore::EMaterialPass& outPass) -> bool {
            auto EqualsI = [](FStringView lhs, const TChar* rhs) -> bool {
                if (rhs == nullptr) {
                    return false;
                }
                const FStringView rhsView(rhs);
                if (lhs.Length() != rhsView.Length()) {
                    return false;
                }
                for (usize i = 0; i < lhs.Length(); ++i) {
                    if (Core::Algorithm::ToLowerChar(lhs[i])
                        != Core::Algorithm::ToLowerChar(rhsView[i])) {
                        return false;
                    }
                }
                return true;
            };

            if (EqualsI(name, TEXT("BasePass"))) {
                outPass = RenderCore::EMaterialPass::BasePass;
                return true;
            }
            if (EqualsI(name, TEXT("DepthPass"))) {
                outPass = RenderCore::EMaterialPass::DepthPass;
                return true;
            }
            if (EqualsI(name, TEXT("ShadowPass"))) {
                outPass = RenderCore::EMaterialPass::ShadowPass;
                return true;
            }
            return false;
        }

        auto IsMaterialCBufferName(FStringView name) -> bool {
            if (name.IsEmpty()) {
                return false;
            }
            const FStringView target(TEXT("MaterialConstants"));
            if (name.Length() < target.Length()) {
                return false;
            }
            for (usize i = 0U; i < target.Length(); ++i) {
                if (Core::Algorithm::ToLowerChar(name[i])
                    != Core::Algorithm::ToLowerChar(target[i])) {
                    return false;
                }
            }
            return true;
        }

        auto FindMaterialCBuffer(const Shader::FShaderReflection& reflection)
            -> const Shader::FShaderConstantBuffer* {
            for (const auto& cbuffer : reflection.mConstantBuffers) {
                if (IsMaterialCBufferName(cbuffer.mName.ToView())) {
                    return &cbuffer;
                }
            }
            return nullptr;
        }

        auto SelectMaterialCBuffer(const Shader::FShaderReflection* vertex,
            const Shader::FShaderReflection* pixel) -> const Shader::FShaderConstantBuffer* {
            const Shader::FShaderConstantBuffer* materialCBuffer = nullptr;
            if (pixel != nullptr) {
                materialCBuffer = FindMaterialCBuffer(*pixel);
            }
            if (materialCBuffer == nullptr && vertex != nullptr) {
                materialCBuffer = FindMaterialCBuffer(*vertex);
            }
            return materialCBuffer;
        }

        auto FindSamplerBinding(
            const Shader::FShaderReflection& reflection, const FStringView& textureName) -> u32 {
            for (const auto& resource : reflection.mResources) {
                if (resource.mType != Shader::EShaderResourceType::Sampler) {
                    continue;
                }
                if (resource.mName.ToView() == textureName) {
                    return resource.mBinding;
                }
            }

            FString withSampler(textureName);
            withSampler.Append(TEXT("Sampler"));
            for (const auto& resource : reflection.mResources) {
                if (resource.mType != Shader::EShaderResourceType::Sampler) {
                    continue;
                }
                if (resource.mName.ToView() == withSampler.ToView()) {
                    return resource.mBinding;
                }
            }

            FString withState(textureName);
            withState.Append(TEXT("SamplerState"));
            for (const auto& resource : reflection.mResources) {
                if (resource.mType != Shader::EShaderResourceType::Sampler) {
                    continue;
                }
                if (resource.mName.ToView() == withState.ToView()) {
                    return resource.mBinding;
                }
            }

            return RenderCore::kMaterialInvalidBinding;
        }

        void AddTextureBindings(
            RenderCore::FMaterialLayout& layout, const Shader::FShaderReflection* reflection) {
            if (reflection == nullptr) {
                return;
            }
            for (const auto& resource : reflection->mResources) {
                if (resource.mType != Shader::EShaderResourceType::Texture) {
                    continue;
                }
                const auto nameHash = RenderCore::HashMaterialParamName(resource.mName.ToView());
                if (nameHash == 0U) {
                    continue;
                }
                const u32 samplerBinding = FindSamplerBinding(*reflection, resource.mName.ToView());
                layout.AddTextureBinding(nameHash, resource.mBinding, samplerBinding);
            }
        }

        auto BuildMaterialLayout(const Shader::FShaderReflection* vertex,
            const Shader::FShaderReflection* pixel) -> RenderCore::FMaterialLayout {
            RenderCore::FMaterialLayout layout;
            const auto*                 materialCBuffer = SelectMaterialCBuffer(vertex, pixel);
            if (materialCBuffer == nullptr) {
                return layout;
            }
            layout.InitFromConstantBuffer(*materialCBuffer);
            AddTextureBindings(layout, pixel);
            AddTextureBindings(layout, vertex);
            layout.SortTextureBindings();
            return layout;
        }

        void LogMaterialLayout(const RenderCore::FMaterialLayout& layout,
            const Shader::FShaderConstantBuffer* materialCBuffer, const FString& passName) {
            LogInfo(TEXT("Material Layout for pass {}"), passName.CStr());

            if (!layout.PropertyBag.IsValid()) {
                LogInfo(TEXT("  PropertyBag: <invalid>"));
            } else {
                LogInfo(
                    TEXT("  PropertyBag: Name={} Size={} Set={} Binding={} Register={} Space={}"),
                    layout.PropertyBag.GetName().CStr(), layout.PropertyBag.GetSizeBytes(),
                    layout.PropertyBag.GetSet(), layout.PropertyBag.GetBinding(),
                    layout.PropertyBag.GetRegister(), layout.PropertyBag.GetSpace());
            }

            if (materialCBuffer == nullptr) {
                LogWarning(TEXT("  Material CBuffer: <null>"));
            } else {
                LogInfo(
                    TEXT(
                        "  Material CBuffer: Name={} Size={} Set={} Binding={} Register={} Space={}"),
                    materialCBuffer->mName.CStr(), materialCBuffer->mSizeBytes,
                    materialCBuffer->mSet, materialCBuffer->mBinding, materialCBuffer->mRegister,
                    materialCBuffer->mSpace);

                LogInfo(
                    TEXT("  Properties: {}"), static_cast<u32>(materialCBuffer->mMembers.Size()));
                for (const auto& member : materialCBuffer->mMembers) {
                    const auto nameHash = RenderCore::HashMaterialParamName(member.mName.ToView());
                    LogInfo(
                        TEXT("    {} (hash=0x{:08X}) Offset={} Size={} ElemCount={} ElemStride={}"),
                        member.mName.CStr(), nameHash, member.mOffset, member.mSize,
                        member.mElementCount, member.mElementStride);
                }
            }

            const usize textureCount = layout.TextureBindings.Size();
            LogInfo(TEXT("  TextureBindings: {}"), static_cast<u32>(textureCount));
            for (usize i = 0U; i < textureCount; ++i) {
                const u32 nameHash =
                    (i < layout.TextureNameHashes.Size()) ? layout.TextureNameHashes[i] : 0U;
                const u32 samplerBinding = (i < layout.SamplerBindings.Size())
                    ? layout.SamplerBindings[i]
                    : RenderCore::kMaterialInvalidBinding;
                LogInfo(TEXT("    [{}] NameHash=0x{:08X} TextureBinding={} SamplerBinding={}"),
                    static_cast<u32>(i), nameHash, layout.TextureBindings[i], samplerBinding);
            }
        }

        auto WriteTempShaderFile(const Container::FNativeStringView& source, const FUuid& uuid,
            ShaderCompiler::EShaderSourceLanguage language,
            Core::Utility::Filesystem::FPath&     outPath) -> bool {
            auto tempRoot = Core::Utility::Filesystem::GetTempDirectory();
            if (tempRoot.IsEmpty()) {
                return false;
            }
            tempRoot /= TEXT("AltinaEngine");
            tempRoot /= TEXT("Shaders");
            if (!Core::Platform::CreateDirectories(tempRoot.GetString())) {
                return false;
            }

            const auto               uuidText = uuid.ToNativeString();
            Container::FNativeString fileName(uuidText.GetData(), uuidText.Length());
            fileName.Append(
                (language == ShaderCompiler::EShaderSourceLanguage::Slang) ? ".slang" : ".hlsl");
            const auto fileNameText = Core::Utility::String::FromUtf8(fileName);
            outPath                 = tempRoot / fileNameText;

            const auto    outPathUtf8 = Core::Utility::String::ToUtf8Bytes(outPath.GetString());
            std::ofstream file(outPathUtf8.CStr(), std::ios::binary | std::ios::trunc);
            if (!file.good()) {
                return false;
            }
            if (source.Length() > 0) {
                file.write(source.Data(), static_cast<std::streamsize>(source.Length()));
            }
            return file.good();
        }

        auto TryParseRasterState(const Asset::FShaderAsset& shader, Shader::FShaderRasterState& out)
            -> bool {
            ShaderCompiler::FShaderPermutationParseResult parse{};
            const auto                                    sourceText =
                Core::Utility::String::FromUtf8(Container::FNativeString(shader.GetSource()));
            if (!ShaderCompiler::ParseShaderPermutationSource(sourceText.ToView(), parse)) {
                return false;
            }
            if (!parse.mHasRasterState) {
                return false;
            }
            out = parse.mRasterState;
            return true;
        }

        auto ToRhiFormat(const Asset::FTexture2DDesc& desc) -> Rhi::ERhiFormat {
            const bool srgb = desc.SRGB;
            switch (desc.Format) {
                case Asset::kTextureFormatRGBA8:
                    return srgb ? Rhi::ERhiFormat::R8G8B8A8UnormSrgb
                                : Rhi::ERhiFormat::R8G8B8A8Unorm;
                case Asset::kTextureFormatR8:
                case Asset::kTextureFormatRGB8:
                default:
                    return srgb ? Rhi::ERhiFormat::R8G8B8A8UnormSrgb
                                : Rhi::ERhiFormat::R8G8B8A8Unorm;
            }
        }

        auto CreateTextureSrv(const Asset::FTexture2DAsset& asset)
            -> Rhi::FRhiShaderResourceViewRef {
            auto* device = Rhi::RHIGetDevice();
            if (device == nullptr) {
                return {};
            }

            const auto&          assetDesc = asset.GetDesc();
            Rhi::FRhiTextureDesc texDesc{};
            texDesc.mWidth     = assetDesc.Width;
            texDesc.mHeight    = assetDesc.Height;
            texDesc.mMipLevels = (assetDesc.MipCount > 0U) ? assetDesc.MipCount : 1U;
            texDesc.mFormat    = ToRhiFormat(assetDesc);
            texDesc.mBindFlags = Rhi::ERhiTextureBindFlags::ShaderResource;

            auto texture = Rhi::RHICreateTexture(texDesc);
            if (!texture) {
                return {};
            }

            Rhi::FRhiShaderResourceViewDesc viewDesc{};
            viewDesc.mTexture                      = texture.Get();
            viewDesc.mFormat                       = texDesc.mFormat;
            viewDesc.mTextureRange.mBaseMip        = 0U;
            viewDesc.mTextureRange.mMipCount       = texDesc.mMipLevels;
            viewDesc.mTextureRange.mBaseArrayLayer = 0U;
            viewDesc.mTextureRange.mLayerCount     = texDesc.mArrayLayers;
            return device->CreateShaderResourceView(viewDesc);
        }

        auto BuildTemplateOverrides(const Asset::FMeshMaterialParameterBlock& overrides)
            -> RenderCore::FMaterialParameterBlock {
            RenderCore::FMaterialParameterBlock block;
            for (const auto& scalar : overrides.GetScalars()) {
                block.SetScalar(scalar.NameHash, scalar.Value);
            }
            for (const auto& vector : overrides.GetVectors()) {
                block.SetVector(vector.NameHash, vector.Value);
            }
            for (const auto& matrix : overrides.GetMatrices()) {
                block.SetMatrix(matrix.NameHash, matrix.Value);
            }
            return block;
        }

        auto HasTemplateOverrides(const RenderCore::FMaterialParameterBlock& overrides) -> bool {
            return !overrides.GetScalars().IsEmpty() || !overrides.GetVectors().IsEmpty()
                || !overrides.GetMatrices().IsEmpty();
        }
    } // namespace

    auto CompileShaderFromAsset(const Asset::FAssetHandle& handle, FStringView entry,
        Shader::EShaderStage stage, Asset::FAssetRegistry& registry, Asset::FAssetManager& manager,
        RenderCore::FShaderRegistry::FShaderKey& outKey,
        ShaderCompiler::FShaderCompileResult&    outResult) -> bool {
        const auto* desc = registry.GetDesc(handle);
        if (desc == nullptr) {
            LogError(TEXT("Shader asset desc missing."));
            return false;
        }

        auto  asset       = manager.Load(handle);
        auto* shaderAsset = asset ? static_cast<Asset::FShaderAsset*>(asset.Get()) : nullptr;
        if (shaderAsset == nullptr) {
            LogError(TEXT("Failed to load shader asset."));
            return false;
        }

        ShaderCompiler::EShaderSourceLanguage language =
            ShaderCompiler::EShaderSourceLanguage::Hlsl;
        if (shaderAsset->GetLanguage() == Asset::kShaderLanguageSlang) {
            language = ShaderCompiler::EShaderSourceLanguage::Slang;
        }

        Core::Utility::Filesystem::FPath tempPath;
        if (!WriteTempShaderFile(shaderAsset->GetSource(), handle.Uuid, language, tempPath)) {
            LogError(TEXT("Failed to write temp shader file."));
            return false;
        }

        ShaderCompiler::FShaderCompileRequest request{};
        request.mSource.mPath.Assign(tempPath.GetString().ToView());
        request.mSource.mEntryPoint.Assign(entry);
        request.mSource.mStage    = stage;
        request.mSource.mLanguage = language;
        if (!tempPath.ParentPath().IsEmpty()) {
            request.mSource.mIncludeDirs.PushBack(tempPath.ParentPath().GetString());
        }
        request.mOptions.mTargetBackend = Rhi::ERhiBackend::DirectX11;
        request.mOptions.mOptimization  = ShaderCompiler::EShaderOptimization::Default;
        request.mOptions.mDebugInfo     = false;

        auto& compiler = ShaderCompiler::GetShaderCompiler();
        outResult      = compiler.Compile(request);

        Core::Platform::RemoveFileIfExists(tempPath.GetString());

        if (!outResult.mSucceeded) {
            LogError(TEXT("Shader compile failed: {}"), outResult.mDiagnostics.ToView());
            return false;
        }

        auto* device = Rhi::RHIGetDevice();
        if (!device) {
            LogError(TEXT("RHI device missing for shader creation."));
            return false;
        }

        auto shaderDesc = ShaderCompiler::BuildRhiShaderDesc(outResult);
        shaderDesc.mDebugName.Assign(entry);
        auto shader = device->CreateShader(shaderDesc);
        if (!shader) {
            LogError(TEXT("Failed to create RHI shader."));
            return false;
        }

        outKey =
            RenderCore::FShaderRegistry::MakeAssetKey(desc->VirtualPath.ToView(), entry, stage);
        if (!Rendering::FBasicDeferredRenderer::RegisterShader(outKey, shader)) {
            LogError(TEXT("Failed to register shader for {}."), outKey.Name.ToView());
            return false;
        }
        return true;
    }

    auto BuildMaterialTemplateFromAsset(const Asset::FMaterialAsset& asset,
        Asset::FAssetRegistry& registry, Asset::FAssetManager& manager)
        -> Container::TShared<RenderCore::FMaterialTemplate> {
        auto templ = Container::MakeShared<RenderCore::FMaterialTemplate>();

        for (const auto& pass : asset.GetPasses()) {
            RenderCore::EMaterialPass passType{};
            if (!TryParseMaterialPass(pass.Name.ToView(), passType)) {
                continue;
            }

            RenderCore::FMaterialPassDesc        passDesc{};
            ShaderCompiler::FShaderCompileResult vertexResult{};
            ShaderCompiler::FShaderCompileResult pixelResult{};
            bool                                 hasVertexResult = false;
            bool                                 hasPixelResult  = false;
            Shader::FShaderRasterState           rasterState{};
            bool                                 hasRasterState = false;

            if (pass.HasVertex) {
                RenderCore::FShaderRegistry::FShaderKey key{};
                if (!CompileShaderFromAsset(pass.Vertex.Asset, pass.Vertex.Entry.ToView(),
                        Shader::EShaderStage::Vertex, registry, manager, key, vertexResult)) {
                    return {};
                }
                passDesc.Shaders.Vertex = key;
                hasVertexResult         = true;
            }

            if (pass.HasPixel) {
                RenderCore::FShaderRegistry::FShaderKey key{};
                if (!CompileShaderFromAsset(pass.Pixel.Asset, pass.Pixel.Entry.ToView(),
                        Shader::EShaderStage::Pixel, registry, manager, key, pixelResult)) {
                    return {};
                }
                passDesc.Shaders.Pixel = key;
                hasPixelResult         = true;
            }

            if (pass.HasCompute) {
                RenderCore::FShaderRegistry::FShaderKey key{};
                ShaderCompiler::FShaderCompileResult    computeResult{};
                if (!CompileShaderFromAsset(pass.Compute.Asset, pass.Compute.Entry.ToView(),
                        Shader::EShaderStage::Compute, registry, manager, key, computeResult)) {
                    return {};
                }
                passDesc.Shaders.Compute = key;
            }

            const Shader::FShaderReflection* vertexReflection =
                hasVertexResult ? &vertexResult.mReflection : nullptr;
            const Shader::FShaderReflection* pixelReflection =
                hasPixelResult ? &pixelResult.mReflection : nullptr;
            passDesc.Layout = BuildMaterialLayout(vertexReflection, pixelReflection);
            LogMaterialLayout(passDesc.Layout,
                SelectMaterialCBuffer(vertexReflection, pixelReflection), pass.Name);

            auto* rasterSourceAsset = static_cast<Asset::FShaderAsset*>(nullptr);
            if (pass.HasPixel) {
                auto assetRef = manager.Load(pass.Pixel.Asset);
                rasterSourceAsset =
                    assetRef ? static_cast<Asset::FShaderAsset*>(assetRef.Get()) : nullptr;
            }
            if (rasterSourceAsset == nullptr && pass.HasVertex) {
                auto assetRef = manager.Load(pass.Vertex.Asset);
                rasterSourceAsset =
                    assetRef ? static_cast<Asset::FShaderAsset*>(assetRef.Get()) : nullptr;
            }
            if (rasterSourceAsset != nullptr) {
                hasRasterState = TryParseRasterState(*rasterSourceAsset, rasterState);
            }

            if (passType == RenderCore::EMaterialPass::BasePass
                || passType == RenderCore::EMaterialPass::DepthPass
                || passType == RenderCore::EMaterialPass::ShadowPass) {
                passDesc.State.Depth.mDepthEnable  = true;
                passDesc.State.Depth.mDepthWrite   = true;
                passDesc.State.Depth.mDepthCompare = Rhi::ERhiCompareOp::LessEqual;
            }

            if (hasRasterState) {
                passDesc.State.ApplyRasterState(rasterState);
            }

            templ->SetPassDesc(passType, Move(passDesc));

            const auto overrideBlock = BuildTemplateOverrides(pass.Overrides);
            if (HasTemplateOverrides(overrideBlock)) {
                templ->SetPassOverrides(passType, overrideBlock);
            }
        }
        if (templ->GetPasses().empty()) {
            return {};
        }
        return templ;
    }

    auto BuildRenderMaterialFromAsset(const Asset::FAssetHandle& handle,
        const Asset::FMeshMaterialParameterBlock& parameters, Asset::FAssetRegistry& registry,
        Asset::FAssetManager& manager) -> RenderCore::FMaterial {
        RenderCore::FMaterial material;
        if (!handle.IsValid() || handle.Type != Asset::EAssetType::MaterialTemplate) {
            return material;
        }

        auto  assetRef = manager.Load(handle);
        auto* materialAsset =
            assetRef ? static_cast<Asset::FMaterialAsset*>(assetRef.Get()) : nullptr;
        if (materialAsset == nullptr) {
            LogError(TEXT("Failed to load material template asset."));
            return material;
        }

        auto templ = BuildMaterialTemplateFromAsset(*materialAsset, registry, manager);
        if (!templ) {
            LogError(TEXT("Failed to build material template from asset."));
            return material;
        }

        material.SetTemplate(templ);

        auto schema       = Container::MakeShared<RenderCore::FMaterialSchema>();
        auto ensureSchema = [&](RenderCore::FMaterialParamId   id,
                                RenderCore::EMaterialParamType type) {
            if (id == 0U) {
                return;
            }
            if (schema->Find(id) != nullptr) {
                return;
            }
            switch (type) {
                case RenderCore::EMaterialParamType::Scalar:
                    schema->AddScalar(id);
                    break;
                case RenderCore::EMaterialParamType::Vector:
                    schema->AddVector(id);
                    break;
                case RenderCore::EMaterialParamType::Matrix:
                    schema->AddMatrix(id);
                    break;
                case RenderCore::EMaterialParamType::Texture:
                    schema->AddTexture(id);
                    break;
            }
        };

        for (const auto& param : parameters.GetScalars()) {
            ensureSchema(param.NameHash, RenderCore::EMaterialParamType::Scalar);
        }
        for (const auto& param : parameters.GetVectors()) {
            ensureSchema(param.NameHash, RenderCore::EMaterialParamType::Vector);
        }
        for (const auto& param : parameters.GetMatrices()) {
            ensureSchema(param.NameHash, RenderCore::EMaterialParamType::Matrix);
        }
        for (const auto& param : parameters.GetTextures()) {
            ensureSchema(param.NameHash, RenderCore::EMaterialParamType::Texture);
        }
        material.SetSchema(Move(schema));

        for (const auto& param : parameters.GetScalars()) {
            material.SetScalar(param.NameHash, param.Value);
        }
        for (const auto& param : parameters.GetVectors()) {
            material.SetVector(param.NameHash, param.Value);
        }
        for (const auto& param : parameters.GetMatrices()) {
            material.SetMatrix(param.NameHash, param.Value);
        }
        for (const auto& param : parameters.GetTextures()) {
            Rhi::FRhiShaderResourceViewRef srv{};
            if (param.Texture.IsValid()
                && param.Type == Asset::EMeshMaterialTextureType::Texture2D) {
                auto  textureAssetRef = manager.Load(param.Texture);
                auto* textureAsset    = textureAssetRef
                       ? static_cast<Asset::FTexture2DAsset*>(textureAssetRef.Get())
                       : nullptr;
                if (textureAsset != nullptr) {
                    srv = CreateTextureSrv(*textureAsset);
                }
            }

            Rhi::FRhiSamplerDesc samplerDesc{};
            samplerDesc.mDebugName.Assign(TEXT("MeshMaterialSampler"));
            auto sampler = Rhi::RHICreateSampler(samplerDesc);
            material.SetTexture(param.NameHash, Move(srv), Move(sampler), param.SamplerFlags);
        }

        auto applyOverrideTexture = [&](const Asset::FMeshMaterialTextureParam& param) {
            if (material.GetParameters().FindTextureParam(param.NameHash) != nullptr) {
                return;
            }
            Rhi::FRhiShaderResourceViewRef srv{};
            if (param.Texture.IsValid()
                && param.Type == Asset::EMeshMaterialTextureType::Texture2D) {
                auto  textureAssetRef = manager.Load(param.Texture);
                auto* textureAsset    = textureAssetRef
                       ? static_cast<Asset::FTexture2DAsset*>(textureAssetRef.Get())
                       : nullptr;
                if (textureAsset != nullptr) {
                    srv = CreateTextureSrv(*textureAsset);
                }
            }

            Rhi::FRhiSamplerDesc samplerDesc{};
            samplerDesc.mDebugName.Assign(TEXT("MeshMaterialSampler"));
            auto sampler = Rhi::RHICreateSampler(samplerDesc);
            material.SetTexture(param.NameHash, Move(srv), Move(sampler), param.SamplerFlags);
        };

        auto applyOverrides = [&](const Asset::FMeshMaterialParameterBlock& overrides) {
            for (const auto& param : overrides.GetScalars()) {
                if (material.GetParameters().FindScalarParam(param.NameHash) != nullptr) {
                    continue;
                }
                ensureSchema(param.NameHash, RenderCore::EMaterialParamType::Scalar);
                material.SetScalar(param.NameHash, param.Value);
            }
            for (const auto& param : overrides.GetVectors()) {
                if (material.GetParameters().FindVectorParam(param.NameHash) != nullptr) {
                    continue;
                }
                ensureSchema(param.NameHash, RenderCore::EMaterialParamType::Vector);
                material.SetVector(param.NameHash, param.Value);
            }
            for (const auto& param : overrides.GetMatrices()) {
                if (material.GetParameters().FindMatrixParam(param.NameHash) != nullptr) {
                    continue;
                }
                ensureSchema(param.NameHash, RenderCore::EMaterialParamType::Matrix);
                material.SetMatrix(param.NameHash, param.Value);
            }
            for (const auto& param : overrides.GetTextures()) {
                if (material.GetParameters().FindTextureParam(param.NameHash) != nullptr) {
                    continue;
                }
                ensureSchema(param.NameHash, RenderCore::EMaterialParamType::Texture);
                applyOverrideTexture(param);
            }
        };

        for (const auto& pass : materialAsset->GetPasses()) {
            applyOverrides(pass.Overrides);
        }

        return material;
    }
} // namespace AltinaEngine::Rendering
