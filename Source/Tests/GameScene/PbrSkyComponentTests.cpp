#include "TestHarness.h"

#include "Engine/EngineReflection.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/PbrSkyComponent.h"
#include "Engine/GameScene/SkyCubeComponent.h"
#include "Engine/GameScene/World.h"
#include "Engine/Runtime/SceneView.h"
#include "Reflection/BinaryDeserializer.h"
#include "Reflection/BinarySerializer.h"
#include "Utility/Uuid.h"

namespace {
    using AltinaEngine::f32;
    using AltinaEngine::u32;
    using AltinaEngine::u8;
    using AltinaEngine::Core::Math::FVector3f;
    using AltinaEngine::Core::Reflection::FBinaryDeserializer;
    using AltinaEngine::Core::Reflection::FBinarySerializer;
    using AltinaEngine::Engine::ESkyProviderType;
    using AltinaEngine::Engine::FRenderScene;
    using AltinaEngine::Engine::FSceneViewBuilder;
    using AltinaEngine::Engine::FSceneViewBuildParams;
    using AltinaEngine::GameScene::FCameraComponent;
    using AltinaEngine::GameScene::FComponentId;
    using AltinaEngine::GameScene::FPbrSkyComponent;
    using AltinaEngine::GameScene::FSkyCubeComponent;
    using AltinaEngine::GameScene::FWorld;

    template <typename TList> auto Contains(const TList& list, FComponentId id) -> bool {
        for (const auto& entry : list) {
            if (entry == id) {
                return true;
            }
        }
        return false;
    }

    auto ReadSerializedString(FBinaryDeserializer& deserializer)
        -> AltinaEngine::Core::Container::FString {
        const u32 length = deserializer.Read<u32>();
        if (length == 0U) {
            return {};
        }

        AltinaEngine::Core::Container::TVector<AltinaEngine::TChar> text{};
        text.Resize(length);
        for (u32 i = 0U; i < length; ++i) {
            text[i] = deserializer.Read<AltinaEngine::TChar>();
        }
        return AltinaEngine::Core::Container::FString(
            text.Data(), static_cast<AltinaEngine::usize>(length));
    }
} // namespace

TEST_CASE("GameScene.PbrSkyComponent.ActiveList") {
    FWorld world;
    auto   obj = world.CreateGameObject(TEXT("Atmosphere"));
    REQUIRE(obj.IsValid());

    auto component = obj.AddComponent<FPbrSkyComponent>();
    REQUIRE(component.IsValid());

    const FComponentId id = component.GetId();
    REQUIRE(Contains(world.GetActivePbrSkyComponents(), id));

    component.Get().SetEnabled(false);
    REQUIRE(!Contains(world.GetActivePbrSkyComponents(), id));

    component.Get().SetEnabled(true);
    REQUIRE(Contains(world.GetActivePbrSkyComponents(), id));

    obj.SetActive(false);
    REQUIRE(!Contains(world.GetActivePbrSkyComponents(), id));

    obj.SetActive(true);
    REQUIRE(Contains(world.GetActivePbrSkyComponents(), id));
}

TEST_CASE("GameScene.PbrSkyComponent.Serialization.BinaryV2Raw") {
    AltinaEngine::Engine::RegisterEngineReflection();

    FWorld world(9);
    auto   obj = world.CreateGameObject(TEXT("Atmosphere"));
    REQUIRE(obj.IsValid());

    auto component = obj.AddComponent<FPbrSkyComponent>();
    REQUIRE(component.IsValid());

    auto& parameters                  = component.Get().mParameters;
    parameters.mExposure              = 1.5f;
    parameters.mSolarIlluminance      = 18.0f;
    parameters.mGroundAlbedo          = AltinaEngine::Core::Math::FVector3f(0.2f, 0.3f, 0.4f);
    parameters.mRayleighScaleHeightKm = 7.0f;
    component.Get().MarkDirty();

    FBinarySerializer serializer;
    world.Serialize(serializer);

    FBinaryDeserializer deserializer;
    deserializer.SetBuffer(serializer.GetBuffer());

    REQUIRE(deserializer.Read<u32>() == 2U);
    REQUIRE(deserializer.Read<u32>() == world.GetWorldId());
    REQUIRE(deserializer.Read<u32>() == 1U);

    REQUIRE(deserializer.Read<u8>() == 0U);
    REQUIRE(deserializer.Read<u32>() == obj.GetId().Index);
    REQUIRE(deserializer.Read<u32>() == obj.GetId().Generation);
    REQUIRE(ReadSerializedString(deserializer).ToView() == TEXT("Atmosphere"));
    REQUIRE(deserializer.Read<bool>() == true);
    REQUIRE(deserializer.Read<bool>() == false);

    for (u32 i = 0U; i < 10U; ++i) {
        (void)deserializer.Read<f32>();
    }

    REQUIRE(deserializer.Read<u32>() == 1U);
    REQUIRE(deserializer.Read<AltinaEngine::GameScene::FComponentTypeHash>()
        == AltinaEngine::GameScene::GetComponentTypeHash<FPbrSkyComponent>());
    REQUIRE(deserializer.Read<bool>() == true);

    REQUIRE_CLOSE(deserializer.Read<f32>(), parameters.mRayleighScattering.X(), 1e-6f);
    REQUIRE_CLOSE(deserializer.Read<f32>(), parameters.mRayleighScattering.Y(), 1e-6f);
    REQUIRE_CLOSE(deserializer.Read<f32>(), parameters.mRayleighScattering.Z(), 1e-6f);
    REQUIRE_CLOSE(deserializer.Read<f32>(), parameters.mRayleighScaleHeightKm, 1e-6f);
}

TEST_CASE("Engine.SceneViewBuilder.SkyProviderSelection") {
    FSceneViewBuilder     builder;
    FSceneViewBuildParams params{};
    params.ViewRect.Width            = 128U;
    params.ViewRect.Height           = 72U;
    params.RenderTargetExtent.Width  = 128U;
    params.RenderTargetExtent.Height = 72U;

    {
        FWorld       world;
        FRenderScene scene{};
        builder.Build(world, params, scene);
        REQUIRE(scene.SkyProvider == ESkyProviderType::None);
        REQUIRE(!scene.bHasSkyCube);
        REQUIRE(!scene.bHasPbrSky);
    }

    {
        FWorld world;
        auto   obj = world.CreateGameObject(TEXT("SkyCube"));
        auto   sky = obj.AddComponent<FSkyCubeComponent>();
        REQUIRE(sky.IsValid());
        AltinaEngine::Asset::FAssetHandle handle{};
        handle.mType = AltinaEngine::Asset::EAssetType::CubeMap;
        handle.mUuid = AltinaEngine::FUuid::New();
        sky.Get().SetCubeMapAsset(handle);

        FRenderScene scene{};
        builder.Build(world, params, scene);
        REQUIRE(scene.SkyProvider == ESkyProviderType::SkyCube);
        REQUIRE(scene.bHasSkyCube);
        REQUIRE(!scene.bHasPbrSky);
    }

    {
        FWorld world;
        auto   obj = world.CreateGameObject(TEXT("PbrSky"));
        auto   sky = obj.AddComponent<FPbrSkyComponent>();
        REQUIRE(sky.IsValid());
        sky.Get().mParameters.mExposure         = 1.25f;
        sky.Get().mParameters.mSolarIlluminance = 25.0f;
        sky.Get().MarkDirty();

        FRenderScene scene{};
        builder.Build(world, params, scene);
        REQUIRE(scene.SkyProvider == ESkyProviderType::PbrSky);
        REQUIRE(!scene.bHasSkyCube);
        REQUIRE(scene.bHasPbrSky);
        REQUIRE_CLOSE(scene.PbrSky.Exposure, 1.25f, 1e-6f);
        REQUIRE_CLOSE(scene.PbrSky.SolarIlluminance, 25.0f, 1e-6f);
    }
}

TEST_CASE("Engine.SceneViewBuilder.InitializesViewMatrices") {
    FWorld world;
    auto   cameraObject = world.CreateGameObject(TEXT("Camera"));
    REQUIRE(cameraObject.IsValid());

    auto camera = cameraObject.AddComponent<FCameraComponent>();
    REQUIRE(camera.IsValid());
    camera.Get().SetNearPlane(0.1f);
    camera.Get().SetFarPlane(250.0f);

    auto transform        = cameraObject.GetWorldTransform();
    transform.Translation = FVector3f(3.0f, 4.0f, 5.0f);
    cameraObject.SetWorldTransform(transform);

    FSceneViewBuildParams params{};
    params.ViewRect.Width            = 1280U;
    params.ViewRect.Height           = 720U;
    params.RenderTargetExtent.Width  = 1280U;
    params.RenderTargetExtent.Height = 720U;
    params.FrameIndex                = 42ULL;

    FSceneViewBuilder builder;
    FRenderScene      scene{};
    builder.Build(world, params, scene);

    REQUIRE_EQ(scene.Views.Size(), 1U);
    REQUIRE_CLOSE(scene.Views[0].View.ViewOrigin.X(), 3.0f, 1e-6f);
    REQUIRE_CLOSE(scene.Views[0].View.ViewOrigin.Y(), 4.0f, 1e-6f);
    REQUIRE_CLOSE(scene.Views[0].View.ViewOrigin.Z(), 5.0f, 1e-6f);
    REQUIRE_CLOSE(scene.Views[0].View.Matrices.ViewProj(3, 2), 1.0f, 1e-6f);
}
