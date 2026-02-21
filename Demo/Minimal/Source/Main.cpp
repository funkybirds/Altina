#include "Base/AltinaBase.h"
#include "Asset/AssetBinary.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Asset/MaterialAsset.h"
#include "Asset/MeshAsset.h"
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
#include "RenderAsset/MeshAssetConversion.h"
#include "RenderAsset/MaterialShaderAssetLoader.h"
#include "ShaderCompiler/ShaderCompiler.h"
#include "Rhi/RhiInit.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"

#include <filesystem>
#include <string>

using namespace AltinaEngine;

namespace {
    namespace Container = Core::Container;
    using Container::FStringView;

    auto BuildStaticMeshFromAsset(
        const Asset::FMeshAsset& asset, RenderCore::Geometry::FStaticMeshData& outMesh) -> bool {
        return Rendering::ConvertMeshAssetToStaticMesh(asset, outMesh);
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

    auto&      assetManager = engineLoop.GetAssetManager();

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

    auto materialTemplate = Rendering::BuildMaterialTemplateFromAsset(
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
    if (!Rendering::CompileShaderFromAsset(shaderHandle, FStringView(TEXT("VSComposite")),
            Shader::EShaderStage::Vertex, engineLoop.GetAssetRegistry(), assetManager, outputVS,
            outputVsResult)
        || !Rendering::CompileShaderFromAsset(shaderHandle, FStringView(TEXT("PSComposite")),
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

    auto cameraObject    = world->CreateGameObject(TEXT("Camera"));
    auto cameraComponent = cameraObject.AddComponent<GameScene::FCameraComponent>();
    if (cameraComponent.IsValid()) {
        auto& camera = cameraComponent.Get();
        camera.SetNearPlane(0.1f);
        camera.SetFarPlane(1000.0f);

        auto transform        = cameraObject.GetWorldTransform();
        transform.Translation = Core::Math::FVector3f(0.0f, 0.0f, -2.0f);
        cameraObject.SetWorldTransform(transform);
    }

    auto meshObject        = world->CreateGameObject(TEXT("TriangleMesh"));
    auto meshComponent     = meshObject.AddComponent<GameScene::FStaticMeshFilterComponent>();
    auto materialComponent = meshObject.AddComponent<GameScene::FMeshMaterialComponent>();

    if (meshComponent.IsValid()) {
        meshComponent.Get().SetStaticMesh(AltinaEngine::Move(meshData));
    }
    if (materialComponent.IsValid()) {
        materialComponent.Get().SetMaterial(0U, material);
    }

    constexpr f32 kFixedDeltaTime = 1.0f / 60.0f;
    for (i32 frameIndex = 0; frameIndex < 600; ++frameIndex) {
        engineLoop.Tick(kFixedDeltaTime);
        Core::Platform::Generic::PlatformSleepMilliseconds(16);
    }

    engineLoop.Exit();
    return 0;
}
