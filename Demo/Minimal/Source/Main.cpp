#include "Base/AltinaBase.h"
#include "Algorithm/CStringUtils.h"
#include "Asset/AssetBinary.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Asset/MaterialAsset.h"
#include "Asset/MeshAsset.h"
#include "Asset/ShaderAsset.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/GameScene/World.h"
#include "Geometry/StaticMeshData.h"
#include "Launch/EngineLoop.h"
#include "Logging/Log.h"
#include "Material/Material.h"
#include "Container/SmartPtr.h"
#include "Math/LinAlg/SpatialTransform.h"
#include "Math/Vector.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Platform/PlatformFileSystem.h"
#include "Rendering/BasicDeferredRenderer.h"
#include "ShaderCompiler/ShaderCompiler.h"
#include "ShaderCompiler/ShaderPermutationParser.h"
#include "ShaderCompiler/ShaderRhiBindings.h"
#include "Rhi/RhiInit.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"
#include "Utility/String/CodeConvert.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <iostream>

using namespace AltinaEngine;

namespace {
    namespace Container = Core::Container;
    using Container::FNativeString;
    using Container::FNativeStringView;
    using Container::FString;
    using Container::FStringView;
    using Container::TVector;

    auto ToFString(const std::filesystem::path& path) -> FString {
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        const std::wstring wide = path.wstring();
        return FString(wide.c_str(), static_cast<usize>(wide.size()));
#else
        const std::string narrow = path.string();
        return FString(narrow.c_str(), static_cast<usize>(narrow.size()));
#endif
    }

    auto FindPositionAttribute(const Asset::TVector<Asset::FMeshVertexAttributeDesc>& attributes,
        u32& outOffset, u32& outFormat) -> bool {
        for (const auto& attr : attributes) {
            if (attr.Semantic == Asset::kMeshSemanticPosition) {
                outOffset = attr.AlignedOffset;
                outFormat = attr.Format;
                return true;
            }
        }
        return false;
    }

    auto ReadPosition(const u8* vertexBase, u32 offset, u32 format) noexcept
        -> Core::Math::FVector3f {
        const auto* data = reinterpret_cast<const f32*>(vertexBase + offset);
        switch (format) {
            case Asset::kMeshVertexFormatR32Float:
                return Core::Math::FVector3f(data[0], 0.0f, 0.0f);
            case Asset::kMeshVertexFormatR32G32Float:
                return Core::Math::FVector3f(data[0], data[1], 0.0f);
            case Asset::kMeshVertexFormatR32G32B32Float:
                return Core::Math::FVector3f(data[0], data[1], data[2]);
            case Asset::kMeshVertexFormatR32G32B32A32Float:
                return Core::Math::FVector3f(data[0], data[1], data[2]);
            default:
                break;
        }
        return Core::Math::FVector3f(0.0f);
    }

    auto BuildStaticMeshFromAsset(
        const Asset::FMeshAsset& asset, RenderCore::Geometry::FStaticMeshData& outMesh) -> bool {
        const auto& desc       = asset.GetDesc();
        const auto& attributes = asset.GetAttributes();
        const auto& subMeshes  = asset.GetSubMeshes();
        const auto& vertices   = asset.GetVertexData();
        const auto& indices    = asset.GetIndexData();

        if (desc.VertexCount == 0U || desc.IndexCount == 0U || desc.VertexStride == 0U) {
            return false;
        }

        u32 positionOffset = 0U;
        u32 positionFormat = 0U;
        if (!FindPositionAttribute(attributes, positionOffset, positionFormat)) {
            return false;
        }

        const u32 positionBytes = [positionFormat]() -> u32 {
            switch (positionFormat) {
                case Asset::kMeshVertexFormatR32Float:
                    return 4U;
                case Asset::kMeshVertexFormatR32G32Float:
                    return 8U;
                case Asset::kMeshVertexFormatR32G32B32Float:
                    return 12U;
                case Asset::kMeshVertexFormatR32G32B32A32Float:
                    return 16U;
                default:
                    return 0U;
            }
        }();

        if (positionBytes == 0U) {
            return false;
        }

        const u64 vertexBytes =
            static_cast<u64>(desc.VertexStride) * static_cast<u64>(desc.VertexCount);
        if (vertices.Size() < vertexBytes) {
            return false;
        }

        const u64 posEnd = static_cast<u64>(positionOffset) + static_cast<u64>(positionBytes);
        if (posEnd > desc.VertexStride) {
            return false;
        }

        TVector<Core::Math::FVector3f> positions;
        positions.Reserve(static_cast<usize>(desc.VertexCount));
        for (u32 i = 0U; i < desc.VertexCount; ++i) {
            const u64 base = static_cast<u64>(i) * static_cast<u64>(desc.VertexStride);
            positions.PushBack(
                ReadPosition(vertices.Data() + base, positionOffset, positionFormat));
        }

        Rhi::ERhiIndexType indexType = Rhi::ERhiIndexType::Uint16;
        switch (desc.IndexType) {
            case Asset::kMeshIndexTypeUint16:
                indexType = Rhi::ERhiIndexType::Uint16;
                break;
            case Asset::kMeshIndexTypeUint32:
                indexType = Rhi::ERhiIndexType::Uint32;
                break;
            default:
                return false;
        }

        const u32 indexStride =
            RenderCore::Geometry::FStaticMeshLodData::GetIndexStrideBytes(indexType);
        if (indexStride == 0U) {
            return false;
        }

        const u64 indexBytes = static_cast<u64>(desc.IndexCount) * static_cast<u64>(indexStride);
        if (indices.Size() < indexBytes) {
            return false;
        }

        RenderCore::Geometry::FStaticMeshData mesh;
        mesh.Lods.Reserve(1);
        auto& lod             = mesh.Lods.EmplaceBack();
        lod.PrimitiveTopology = Rhi::ERhiPrimitiveTopology::TriangleList;
        lod.SetPositions(positions.Data(), desc.VertexCount);
        lod.SetIndices(indices.Data(), desc.IndexCount, indexType);

        if (!subMeshes.IsEmpty()) {
            lod.Sections.Reserve(subMeshes.Size());
            for (const auto& subMesh : subMeshes) {
                RenderCore::Geometry::FStaticMeshSection section{};
                section.FirstIndex   = subMesh.IndexStart;
                section.IndexCount   = subMesh.IndexCount;
                section.BaseVertex   = subMesh.BaseVertex;
                section.MaterialSlot = subMesh.MaterialSlot;
                lod.Sections.PushBack(section);
            }
        } else {
            RenderCore::Geometry::FStaticMeshSection section{};
            section.FirstIndex   = 0U;
            section.IndexCount   = desc.IndexCount;
            section.BaseVertex   = 0;
            section.MaterialSlot = 0U;
            lod.Sections.PushBack(section);
        }

        lod.Bounds.Min =
            Core::Math::FVector3f(desc.BoundsMin[0], desc.BoundsMin[1], desc.BoundsMin[2]);
        lod.Bounds.Max =
            Core::Math::FVector3f(desc.BoundsMax[0], desc.BoundsMax[1], desc.BoundsMax[2]);

        mesh.Bounds = lod.Bounds;

        if (!mesh.IsValid()) {
            return false;
        }

        outMesh = AltinaEngine::Move(mesh);
        return true;
    }

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
            if (Core::Algorithm::ToLowerChar(name[i]) != Core::Algorithm::ToLowerChar(target[i])) {
                return false;
            }
        }
        return true;
    }

    auto FindMaterialCBuffer(const Shader::FShaderReflection& reflection)
        -> const Shader::FShaderConstantBuffer* {
        for (const auto& cbuffer : reflection.mConstantBuffers) {
            if (IsMaterialCBufferName(cbuffer.mName.ToView())) {
                std::cout << "Addr=" << &cbuffer << std::endl;
                return &cbuffer;
            }
        }
        return nullptr;
    }

    auto SelectMaterialCBuffer(const Shader::FShaderReflection* vertex,
        const Shader::FShaderReflection* pixel) -> const Shader::FShaderConstantBuffer* {
        const Shader::FShaderConstantBuffer* materialCBuffer = nullptr;
        if (pixel) {
            materialCBuffer = FindMaterialCBuffer(*pixel);
        }
        if (!materialCBuffer && vertex) {
            materialCBuffer = FindMaterialCBuffer(*vertex);
        }
        if (!materialCBuffer && pixel && !pixel->mConstantBuffers.IsEmpty()) {
            materialCBuffer = &pixel->mConstantBuffers[0];
        }
        if (!materialCBuffer && vertex && !vertex->mConstantBuffers.IsEmpty()) {
            materialCBuffer = &vertex->mConstantBuffers[0];
        }
        return materialCBuffer;
    }

    auto BuildMaterialLayout(const Shader::FShaderReflection* vertex,
        const Shader::FShaderReflection* pixel) -> RenderCore::FMaterialLayout {
        RenderCore::FMaterialLayout layout;
        const auto*                 materialCBuffer = SelectMaterialCBuffer(vertex, pixel);
        if (materialCBuffer != nullptr) {
            layout.InitFromConstantBuffer(*materialCBuffer);
        }
        return layout;
    }

    void LogMaterialLayout(const RenderCore::FMaterialLayout& layout,
        const Shader::FShaderConstantBuffer* materialCBuffer, const FString& passName) {
        LogInfo(TEXT("Material Layout for pass {}"), passName.CStr());

        if (!layout.PropertyBag.IsValid()) {
            LogInfo(TEXT("  PropertyBag: <invalid>"));
        } else {
            LogInfo(TEXT("  PropertyBag: Name={} Size={} Set={} Binding={} Register={} Space={}"),
                layout.PropertyBag.GetName().CStr(), layout.PropertyBag.GetSizeBytes(),
                layout.PropertyBag.GetSet(), layout.PropertyBag.GetBinding(),
                layout.PropertyBag.GetRegister(), layout.PropertyBag.GetSpace());
        }

        if (materialCBuffer == nullptr) {
            LogWarning(TEXT("  Material CBuffer: <null>"));
        } else {
            LogInfo(
                TEXT("  Material CBuffer: Name={} Size={} Set={} Binding={} Register={} Space={}"),
                materialCBuffer->mName.CStr(), materialCBuffer->mSizeBytes, materialCBuffer->mSet,
                materialCBuffer->mBinding, materialCBuffer->mRegister, materialCBuffer->mSpace);

            LogInfo(TEXT("  Properties: {}"), static_cast<u32>(materialCBuffer->mMembers.Size()));
            for (const auto& member : materialCBuffer->mMembers) {
                const auto nameHash = RenderCore::HashMaterialParamName(member.mName.ToView());
                LogInfo(TEXT("    {} (hash=0x{:08X}) Offset={} Size={} ElemCount={} ElemStride={}"),
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

    auto WriteTempShaderFile(const FNativeStringView& source, const FUuid& uuid,
        ShaderCompiler::EShaderSourceLanguage language, std::filesystem::path& outPath) -> bool {
        std::error_code       ec;
        std::filesystem::path tempRoot = std::filesystem::temp_directory_path(ec);
        if (ec) {
            return false;
        }
        tempRoot /= "AltinaEngine";
        tempRoot /= "Shaders";
        std::filesystem::create_directories(tempRoot, ec);
        if (ec) {
            return false;
        }

        const auto  uuidText = uuid.ToNativeString();
        std::string fileName(uuidText.GetData(), uuidText.Length());
        fileName += (language == ShaderCompiler::EShaderSourceLanguage::Slang) ? ".slang" : ".hlsl";
        outPath = tempRoot / fileName;

        std::ofstream file(outPath, std::ios::binary | std::ios::trunc);
        if (!file.good()) {
            return false;
        }
        if (source.Length() > 0) {
            file.write(source.Data(), static_cast<std::streamsize>(source.Length()));
        }
        return file.good();
    }

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

        std::filesystem::path tempPath;
        if (!WriteTempShaderFile(shaderAsset->GetSource(), handle.Uuid, language, tempPath)) {
            LogError(TEXT("Failed to write temp shader file."));
            return false;
        }

        ShaderCompiler::FShaderCompileRequest request{};
        request.mSource.mPath.Assign(ToFString(tempPath).ToView());
        request.mSource.mEntryPoint.Assign(entry);
        request.mSource.mStage    = stage;
        request.mSource.mLanguage = language;
        if (tempPath.has_parent_path()) {
            request.mSource.mIncludeDirs.PushBack(ToFString(tempPath.parent_path()));
        }
        request.mOptions.mTargetBackend = Rhi::ERhiBackend::DirectX11;
        request.mOptions.mOptimization  = ShaderCompiler::EShaderOptimization::Default;
        request.mOptions.mDebugInfo     = false;

        auto& compiler = ShaderCompiler::GetShaderCompiler();
        outResult      = compiler.Compile(request);

        std::error_code removeEc;
        std::filesystem::remove(tempPath, removeEc);

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
        }
        if (templ->GetPasses().empty()) {
            return {};
        }
        return templ;
    }
} // namespace

int main(int argc, char** argv) {
    FStartupParameters startupParams{};
    if (argc > 1) {
        startupParams.mCommandLine = argv[1];
    }

    Launch::FEngineLoop engineLoop(startupParams);
    if (!engineLoop.PreInit()) {
        return 1;
    }
    if (!engineLoop.Init()) {
        engineLoop.Exit();
        return 1;
    }

    auto& assetManager = engineLoop.GetAssetManager();

    const auto meshHandle = engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/minimal/triangle"));
    const auto materialHandle =
        engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/minimal/materials/purpledeferred"));
    const auto shaderHandle =
        engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/minimal/shaders/basicdeferred"));
    if (!meshHandle.IsValid() || !materialHandle.IsValid() || !shaderHandle.IsValid()) {
        LogError(TEXT("Demo assets missing (mesh, material, or shader)."));
        engineLoop.Exit();
        return 1;
    }

    auto  meshAsset = assetManager.Load(meshHandle);
    auto* mesh      = meshAsset ? static_cast<Asset::FMeshAsset*>(meshAsset.Get()) : nullptr;
    if (mesh == nullptr) {
        LogError(TEXT("Failed to load mesh asset."));
        engineLoop.Exit();
        return 1;
    }

    RenderCore::Geometry::FStaticMeshData meshData;
    if (!BuildStaticMeshFromAsset(*mesh, meshData)) {
        LogError(TEXT("Failed to build static mesh data from asset."));
        engineLoop.Exit();
        return 1;
    }
    for (auto& lod : meshData.Lods) {
        lod.PositionBuffer.InitResource();
        lod.IndexBuffer.InitResource();
        lod.TangentBuffer.InitResource();
        lod.UV0Buffer.InitResource();
        lod.UV1Buffer.InitResource();

        lod.PositionBuffer.WaitForInit();
        lod.IndexBuffer.WaitForInit();
        lod.TangentBuffer.WaitForInit();
        lod.UV0Buffer.WaitForInit();
        lod.UV1Buffer.WaitForInit();
    }

    auto  materialAsset = assetManager.Load(materialHandle);
    auto* materialTemplateAsset =
        materialAsset ? static_cast<Asset::FMaterialAsset*>(materialAsset.Get()) : nullptr;
    if (materialTemplateAsset == nullptr) {
        LogError(TEXT("Failed to load material template asset."));
        engineLoop.Exit();
        return 1;
    }

    auto materialTemplate = BuildMaterialTemplateFromAsset(
        *materialTemplateAsset, engineLoop.GetAssetRegistry(), assetManager);
    if (!materialTemplate) {
        LogError(TEXT("Failed to build material template."));
        engineLoop.Exit();
        return 1;
    }

    RenderCore::FShaderRegistry::FShaderKey outputVS{};
    RenderCore::FShaderRegistry::FShaderKey outputPS{};
    ShaderCompiler::FShaderCompileResult    outputVsResult{};
    ShaderCompiler::FShaderCompileResult    outputPsResult{};
    if (!CompileShaderFromAsset(shaderHandle, FStringView(TEXT("VSComposite")),
            Shader::EShaderStage::Vertex, engineLoop.GetAssetRegistry(), assetManager, outputVS,
            outputVsResult)
        || !CompileShaderFromAsset(shaderHandle, FStringView(TEXT("PSComposite")),
            Shader::EShaderStage::Pixel, engineLoop.GetAssetRegistry(), assetManager, outputPS,
            outputPsResult)) {
        LogError(TEXT("Failed to compile output shaders."));
        engineLoop.Exit();
        return 1;
    }

    Rendering::FBasicDeferredRenderer::SetDefaultMaterialTemplate(materialTemplate);
    Rendering::FBasicDeferredRenderer::SetOutputShaderKeys(outputVS, outputPS);

    auto material = Container::MakeShared<RenderCore::FMaterial>();
    material->SetTemplate(materialTemplate);
    const auto baseColorId = RenderCore::HashMaterialParamName(TEXT("BaseColor"));
    material->SetVector(baseColorId, Core::Math::FVector4f(1.0f, 0.0f, 1.0f, 1.0f));

    auto&      worldManager = engineLoop.GetWorldManager();
    const auto worldHandle  = worldManager.CreateWorld();
    worldManager.SetActiveWorld(worldHandle);
    auto* world = worldManager.GetWorld(worldHandle);
    if (world == nullptr) {
        LogError(TEXT("Demo world creation failed."));
        engineLoop.Exit();
        return 1;
    }

    const auto cameraObject = world->CreateGameObject(TEXT("Camera"));
    const auto cameraComponentId =
        world->CreateComponent<GameScene::FCameraComponent>(cameraObject);
    if (cameraComponentId.IsValid()) {
        auto& camera = world->ResolveComponent<GameScene::FCameraComponent>(cameraComponentId);
        camera.SetNearPlane(0.1f);
        camera.SetFarPlane(1000.0f);

        auto cameraView       = world->Object(cameraObject);
        auto transform        = cameraView.GetWorldTransform();
        transform.Translation = Core::Math::FVector3f(0.0f, 0.0f, -2.0f);
        cameraView.SetWorldTransform(transform);
    }

    const auto meshObject = world->CreateGameObject(TEXT("TriangleMesh"));
    const auto meshComponentId =
        world->CreateComponent<GameScene::FStaticMeshFilterComponent>(meshObject);
    const auto materialComponentId =
        world->CreateComponent<GameScene::FMeshMaterialComponent>(meshObject);

    if (meshComponentId.IsValid()) {
        auto& meshComponent =
            world->ResolveComponent<GameScene::FStaticMeshFilterComponent>(meshComponentId);
        meshComponent.SetStaticMesh(AltinaEngine::Move(meshData));
    }
    if (materialComponentId.IsValid()) {
        auto& materialComponent =
            world->ResolveComponent<GameScene::FMeshMaterialComponent>(materialComponentId);
        materialComponent.SetMaterial(0U, material);
    }

    constexpr f32 kFixedDeltaTime = 1.0f / 60.0f;
    for (i32 frameIndex = 0; frameIndex < 600; ++frameIndex) {
        engineLoop.Tick(kFixedDeltaTime);
        Core::Platform::Generic::PlatformSleepMilliseconds(16);
    }

    engineLoop.Exit();
    return 0;
}
