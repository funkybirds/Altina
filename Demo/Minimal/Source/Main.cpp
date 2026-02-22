#include "Base/AltinaBase.h"
#include "Asset/AssetBinary.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Asset/MaterialAsset.h"
#include "Asset/MeshMaterialParameterBlock.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/GameScene/World.h"
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
#include "Reflection/JsonSerializer.h"
#include "Rhi/RhiInit.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"

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

            auto  materialAsset = assetManager.Load(materialHandle);
            auto* materialTemplateAsset =
                materialAsset ? static_cast<Asset::FMaterialAsset*>(materialAsset.Get()) : nullptr;
            if (materialTemplateAsset == nullptr) {
                LogError(TEXT("Failed to load material template asset."));
                return false;
            }

            // const auto baseColorId = RenderCore::HashMaterialParamName(TEXT("BaseColor"));
            // Asset::FMeshMaterialParameterBlock materialParams;
            // materialParams.SetVector(baseColorId, Core::Math::FVector4f(1.0f, 0.0f, 1.0f, 1.0f));

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
                meshComponent.Get().SetStaticMeshAsset(meshHandle);
            }
            if (materialComponent.IsValid()) {
                materialComponent.Get().SetMaterialTemplate(0U, materialHandle);
                // materialComponent.Get().SetMaterialParameters(0U, materialParams);
            }

            {
                Core::Reflection::FJsonSerializer serializer;
                world->SerializeJson(serializer);
                LogInfo(TEXT("World JSON: {}"), serializer.GetString().CStr());
            }

            return true;
        }

        auto OnTick(Launch::FEngineLoop& engineLoop, float deltaSeconds) -> bool override {
            engineLoop.Tick(deltaSeconds);
            Core::Platform::Generic::PlatformSleepMilliseconds(16);
            return engineLoop.IsRunning();
        }
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
