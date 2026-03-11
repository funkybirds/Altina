#include "TestHarness.h"

#include "Asset/AssetBinary.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Asset/LevelLoader.h"
#include "Engine/EngineReflection.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/DirectionalLightComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/PointLightComponent.h"
#include "Engine/GameScene/ScriptComponent.h"
#include "Engine/GameScene/SkyCubeComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/GameScene/World.h"
#include "Engine/GameSceneAsset/LevelAssetIO.h"
#include "Reflection/JsonSerializer.h"

#include <cstring>
#include <filesystem>
#include <fstream>

using namespace AltinaEngine;

namespace {
    auto ToFString(const std::filesystem::path& path) -> Core::Container::FString {
        Core::Container::FString out{};
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        const auto wide = path.wstring();
        if (!wide.empty()) {
            out.Append(wide.c_str(), wide.size());
        }
#else
        const auto narrow = path.string();
        if (!narrow.empty()) {
            out.Append(narrow.c_str(), narrow.size());
        }
#endif
        return out;
    }
} // namespace

TEST_CASE("GameScene.World.DeserializeJson.RoundTrip") {
    Engine::RegisterEngineReflection();

    GameScene::FWorld source(701);
    auto              object = source.CreateGameObject(TEXT("Camera"));
    auto              camera = object.AddComponent<GameScene::FCameraComponent>();
    REQUIRE(camera.IsValid());
    camera.Get().SetNearPlane(0.25f);
    camera.Get().SetFarPlane(2500.0f);

    auto directional = object.AddComponent<GameScene::FDirectionalLightComponent>();
    REQUIRE(directional.IsValid());
    directional.Get().mIntensity = 3.5f;

    auto point = object.AddComponent<GameScene::FPointLightComponent>();
    REQUIRE(point.IsValid());
    point.Get().mRange = 64.0f;

    auto mesh = object.AddComponent<GameScene::FStaticMeshFilterComponent>();
    REQUIRE(mesh.IsValid());
    Asset::FAssetHandle meshHandle{};
    meshHandle.mType = Asset::EAssetType::Mesh;
    REQUIRE(
        FUuid::TryParse(Core::Container::FNativeStringView("11111111-2222-3333-4444-555555555555"),
            meshHandle.mUuid));
    mesh.Get().SetStaticMeshAsset(meshHandle);

    auto material = object.AddComponent<GameScene::FMeshMaterialComponent>();
    REQUIRE(material.IsValid());
    Asset::FAssetHandle materialHandle{};
    materialHandle.mType = Asset::EAssetType::MaterialTemplate;
    REQUIRE(
        FUuid::TryParse(Core::Container::FNativeStringView("66666666-7777-8888-9999-aaaaaaaaaaaa"),
            materialHandle.mUuid));
    material.Get().SetMaterialTemplate(0U, materialHandle);

    auto script = object.AddComponent<GameScene::FScriptComponent>();
    REQUIRE(script.IsValid());
    script.Get().SetAssemblyPath("Demo/Managed.dll");
    script.Get().SetTypeName("Demo.TestBehaviour");
    Asset::FAssetHandle scriptHandle{};
    scriptHandle.mType = Asset::EAssetType::Script;
    REQUIRE(
        FUuid::TryParse(Core::Container::FNativeStringView("bbbbbbbb-cccc-dddd-eeee-ffffffffffff"),
            scriptHandle.mUuid));
    script.Get().SetScriptAsset(scriptHandle);

    auto sky = object.AddComponent<GameScene::FSkyCubeComponent>();
    REQUIRE(sky.IsValid());
    Asset::FAssetHandle skyHandle{};
    skyHandle.mType = Asset::EAssetType::CubeMap;
    REQUIRE(
        FUuid::TryParse(Core::Container::FNativeStringView("12345678-90ab-cdef-1234-567890abcdef"),
            skyHandle.mUuid));
    sky.Get().SetCubeMapAsset(skyHandle);

    Core::Reflection::FJsonSerializer serializer{};
    source.SerializeJson(serializer);

    Asset::FAssetManager manager{};
    auto                 loaded = GameScene::FWorld::DeserializeJson(serializer.GetText(), manager);
    REQUIRE(loaded != nullptr);
    REQUIRE_EQ(loaded->GetWorldId(), source.GetWorldId());
    REQUIRE(loaded->IsAlive(object.GetId()));

    const auto loadedCameraId = loaded->GetComponent<GameScene::FCameraComponent>(object.GetId());
    REQUIRE(loadedCameraId.IsValid());
    const auto& loadedCamera =
        loaded->ResolveComponent<GameScene::FCameraComponent>(loadedCameraId);
    REQUIRE(loadedCamera.GetNearPlane() == 0.25f);
    REQUIRE(loadedCamera.GetFarPlane() == 2500.0f);

    const auto loadedDirectionalId =
        loaded->GetComponent<GameScene::FDirectionalLightComponent>(object.GetId());
    REQUIRE(loadedDirectionalId.IsValid());
    REQUIRE(loaded->ResolveComponent<GameScene::FDirectionalLightComponent>(loadedDirectionalId)
                .mIntensity
        == 3.5f);

    const auto loadedPointId =
        loaded->GetComponent<GameScene::FPointLightComponent>(object.GetId());
    REQUIRE(loadedPointId.IsValid());
    REQUIRE(
        loaded->ResolveComponent<GameScene::FPointLightComponent>(loadedPointId).mRange == 64.0f);

    const auto loadedMeshId =
        loaded->GetComponent<GameScene::FStaticMeshFilterComponent>(object.GetId());
    REQUIRE(loadedMeshId.IsValid());
    REQUIRE(loaded->ResolveComponent<GameScene::FStaticMeshFilterComponent>(loadedMeshId)
                .GetStaticMeshAsset()
        == meshHandle);

    const auto loadedMaterialId =
        loaded->GetComponent<GameScene::FMeshMaterialComponent>(object.GetId());
    REQUIRE(loadedMaterialId.IsValid());
    REQUIRE(loaded->ResolveComponent<GameScene::FMeshMaterialComponent>(loadedMaterialId)
                .GetMaterials()[0]
                .Template
        == materialHandle);

    const auto loadedScriptId = loaded->GetComponent<GameScene::FScriptComponent>(object.GetId());
    REQUIRE(loadedScriptId.IsValid());
    REQUIRE(loaded->ResolveComponent<GameScene::FScriptComponent>(loadedScriptId).GetScriptAsset()
        == scriptHandle);

    const auto loadedSkyId = loaded->GetComponent<GameScene::FSkyCubeComponent>(object.GetId());
    REQUIRE(loadedSkyId.IsValid());
    REQUIRE(loaded->ResolveComponent<GameScene::FSkyCubeComponent>(loadedSkyId).GetCubeMapAsset()
        == skyHandle);
}

TEST_CASE("GameScene.LevelAsset.LoadWorldFromJsonEncodedLevel") {
    Engine::RegisterEngineReflection();

    GameScene::FWorld source(702);
    auto              object = source.CreateGameObject(TEXT("Camera"));
    auto              camera = object.AddComponent<GameScene::FCameraComponent>();
    REQUIRE(camera.IsValid());
    camera.Get().SetNearPlane(0.5f);

    Core::Container::FNativeString levelJson{};
    REQUIRE(Engine::GameSceneAsset::SaveWorldAsLevelJson(source, levelJson));

    Asset::FAssetBlobHeader blobHeader{};
    blobHeader.mType     = static_cast<u8>(Asset::EAssetType::Level);
    blobHeader.mDescSize = static_cast<u32>(sizeof(Asset::FLevelBlobDesc));
    blobHeader.mDataSize = static_cast<u32>(levelJson.Length());

    Asset::FLevelBlobDesc levelDesc{};
    levelDesc.mEncoding = Asset::kLevelEncodingWorldJson;

    const auto tempPath = std::filesystem::current_path() / "WorldLevelAssetTests.level.bin";
    {
        std::ofstream file(tempPath, std::ios::binary);
        REQUIRE(file.good());
        file.write(reinterpret_cast<const char*>(&blobHeader),
            static_cast<std::streamsize>(sizeof(blobHeader)));
        file.write(reinterpret_cast<const char*>(&levelDesc),
            static_cast<std::streamsize>(sizeof(levelDesc)));
        file.write(levelJson.CStr(), static_cast<std::streamsize>(levelJson.Length()));
    }

    Asset::FAssetDesc desc{};
    desc.mHandle.mType = Asset::EAssetType::Level;
    REQUIRE(
        FUuid::TryParse(Core::Container::FNativeStringView("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"),
            desc.mHandle.mUuid));
    desc.mVirtualPath    = TEXT("tests/level/default");
    desc.mCookedPath     = ToFString(tempPath);
    desc.mLevel.Encoding = Asset::kLevelEncodingWorldJson;
    desc.mLevel.ByteSize = static_cast<u32>(levelJson.Length());

    Asset::FAssetRegistry registry{};
    registry.AddAsset(desc);

    Asset::FAssetManager manager{};
    Asset::FLevelLoader  levelLoader{};
    manager.SetRegistry(&registry);
    manager.RegisterLoader(&levelLoader);

    const auto loaded = Engine::GameSceneAsset::LoadWorldFromLevelAsset(desc.mHandle, manager);
    REQUIRE(loaded != nullptr);
    REQUIRE_EQ(loaded->GetWorldId(), source.GetWorldId());
    REQUIRE(loaded->IsAlive(object.GetId()));
    const auto loadedCameraId = loaded->GetComponent<GameScene::FCameraComponent>(object.GetId());
    REQUIRE(loadedCameraId.IsValid());
    REQUIRE(loaded->ResolveComponent<GameScene::FCameraComponent>(loadedCameraId).GetNearPlane()
        == 0.5f);

    manager.UnregisterLoader(&levelLoader);
    manager.SetRegistry(nullptr);

    std::error_code ec{};
    std::filesystem::remove(tempPath, ec);
}
