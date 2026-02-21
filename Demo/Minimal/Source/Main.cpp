#include "Base/AltinaBase.h"
#include "Asset/AssetBinary.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Asset/MaterialAsset.h"
#include "Asset/MeshAsset.h"
#include "Asset/MeshMaterialParameterBlock.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/GameScene/World.h"
#include "Geometry/StaticMeshData.h"
#include "Launch/EngineLoop.h"
#include "Launch/GameClient.h"
#include "Logging/Log.h"
#include "Material/MaterialPass.h"
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

} // namespace

namespace {
    class FMinimalGameClient final : public Launch::FGameClient {
    public:
        auto OnInit(Launch::FEngineLoop& engineLoop) -> bool override {
            auto&      assetManager = engineLoop.GetAssetManager();

            const auto meshHandle =
                engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/minimal/triangle"));
            const auto materialHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/minimal/materials/purpledeferred"));
            const auto shaderHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/minimal/shaders/basicdeferred"));
            if (!meshHandle.IsValid() || !materialHandle.IsValid() || !shaderHandle.IsValid()) {
                LogError(TEXT("Demo assets missing (mesh, material, or shader)."));
                return false;
            }

            auto  meshAsset = assetManager.Load(meshHandle);
            auto* mesh = meshAsset ? static_cast<Asset::FMeshAsset*>(meshAsset.Get()) : nullptr;
            if (mesh == nullptr) {
                LogError(TEXT("Failed to load mesh asset."));
                return false;
            }

            RenderCore::Geometry::FStaticMeshData meshData;
            if (!Rendering::ConvertMeshAssetToStaticMesh(*mesh, meshData)) {
                LogError(TEXT("Failed to build static mesh data from asset."));
                return false;
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
                return false;
            }

            auto materialTemplate = Rendering::BuildMaterialTemplateFromAsset(
                *materialTemplateAsset, engineLoop.GetAssetRegistry(), assetManager);
            if (!materialTemplate) {
                LogError(TEXT("Failed to build material template."));
                return false;
            }

            RenderCore::FShaderRegistry::FShaderKey outputVS{};
            RenderCore::FShaderRegistry::FShaderKey outputPS{};
            ShaderCompiler::FShaderCompileResult    outputVsResult{};
            ShaderCompiler::FShaderCompileResult    outputPsResult{};
            if (!Rendering::CompileShaderFromAsset(shaderHandle, FStringView(TEXT("VSComposite")),
                    Shader::EShaderStage::Vertex, engineLoop.GetAssetRegistry(), assetManager,
                    outputVS, outputVsResult)
                || !Rendering::CompileShaderFromAsset(shaderHandle,
                    FStringView(TEXT("PSComposite")), Shader::EShaderStage::Pixel,
                    engineLoop.GetAssetRegistry(), assetManager, outputPS, outputPsResult)) {
                LogError(TEXT("Failed to compile output shaders."));
                return false;
            }

            Rendering::FBasicDeferredRenderer::SetDefaultMaterialTemplate(materialTemplate);
            Rendering::FBasicDeferredRenderer::SetOutputShaderKeys(outputVS, outputPS);

            const auto baseColorId = RenderCore::HashMaterialParamName(TEXT("BaseColor"));
            Asset::FMeshMaterialParameterBlock materialParams;
            materialParams.SetVector(baseColorId, Core::Math::FVector4f(1.0f, 0.0f, 1.0f, 1.0f));

            auto&      worldManager = engineLoop.GetWorldManager();
            const auto worldHandle  = worldManager.CreateWorld();
            worldManager.SetActiveWorld(worldHandle);
            auto* world = worldManager.GetWorld(worldHandle);
            if (world == nullptr) {
                LogError(TEXT("Demo world creation failed."));
                return false;
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

            auto meshObject    = world->CreateGameObject(TEXT("TriangleMesh"));
            auto meshComponent = meshObject.AddComponent<GameScene::FStaticMeshFilterComponent>();
            auto materialComponent = meshObject.AddComponent<GameScene::FMeshMaterialComponent>();

            if (meshComponent.IsValid()) {
                meshComponent.Get().SetStaticMesh(AltinaEngine::Move(meshData));
            }
            if (materialComponent.IsValid()) {
                materialComponent.Get().SetMaterialTemplate(0U, materialHandle);
                materialComponent.Get().SetMaterialParameters(0U, materialParams);
            }

            return true;
        }

        auto OnTick(Launch::FEngineLoop& engineLoop, float deltaSeconds) -> bool override {
            engineLoop.Tick(deltaSeconds);
            Core::Platform::Generic::PlatformSleepMilliseconds(16);
            ++mFrameIndex;
            return mFrameIndex < mMaxFrames;
        }

    private:
        u32 mFrameIndex = 0U;
        u32 mMaxFrames  = 600U;
    };
} // namespace

int main(int argc, char** argv) {
    FStartupParameters startupParams{};
    if (argc > 1) {
        startupParams.mCommandLine = argv[1];
    }

    FMinimalGameClient client;
    return Launch::RunGameClient(client, startupParams);
}
