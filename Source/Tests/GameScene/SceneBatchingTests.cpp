#include "TestHarness.h"

#include "Engine/Runtime/MaterialCache.h"
#include "Engine/Runtime/SceneBatching.h"
#include "Engine/Runtime/SceneView.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Geometry/StaticMeshData.h"
#include "Math/LinAlg/Common.h"
#include "Utility/Uuid.h"

namespace {
    using AltinaEngine::f32;
    using AltinaEngine::FUuid;
    using AltinaEngine::u32;
    using AltinaEngine::u8;
    using AltinaEngine::Asset::EAssetType;
    using AltinaEngine::Asset::FAssetHandle;
    using AltinaEngine::Engine::FMaterialCache;
    using AltinaEngine::Engine::FRenderScene;
    using AltinaEngine::Engine::FSceneBatchBuilder;
    using AltinaEngine::Engine::FSceneBatchBuildParams;
    using AltinaEngine::Engine::FSceneStaticMesh;
    using AltinaEngine::Engine::FSceneView;
    using AltinaEngine::GameScene::FMeshMaterialComponent;
    using AltinaEngine::RenderCore::EMaterialPass;
    using AltinaEngine::RenderCore::FMaterial;
    using AltinaEngine::RenderCore::Geometry::FStaticMeshData;
    using AltinaEngine::RenderCore::Geometry::FStaticMeshSection;
    using AltinaEngine::RenderCore::View::FRenderTargetExtent2D;
    using AltinaEngine::RenderCore::View::FViewRect;
    using AltinaEngine::Rhi::ERhiPrimitiveTopology;

    struct FMaterialConverterGuard {
        FMeshMaterialComponent::FAssetToRenderMaterialConverter mOldConverter;

        explicit FMaterialConverterGuard(
            FMeshMaterialComponent::FAssetToRenderMaterialConverter converter)
            : mOldConverter(FMeshMaterialComponent::AssetToRenderMaterialConverter) {
            FMeshMaterialComponent::AssetToRenderMaterialConverter = converter;
        }

        ~FMaterialConverterGuard() {
            FMeshMaterialComponent::AssetToRenderMaterialConverter = mOldConverter;
        }
    };

    auto MakeMaterialHandle(u8 id) -> FAssetHandle {
        FUuid::FBytes bytes{};
        bytes[0] = static_cast<u8>(id + 1U);

        FAssetHandle handle{};
        handle.mUuid = FUuid(bytes);
        handle.mType = EAssetType::MaterialTemplate;
        return handle;
    }

    auto MakeStaticMesh(u32 materialSlot = 0U) -> FStaticMeshData {
        FStaticMeshData mesh{};
        mesh.mLods.Resize(1U);

        auto& lod              = mesh.mLods[0];
        lod.mPrimitiveTopology = ERhiPrimitiveTopology::TriangleList;
        lod.mBounds.Min        = AltinaEngine::Core::Math::FVector3f(-0.5f, -0.5f, -0.5f);
        lod.mBounds.Max        = AltinaEngine::Core::Math::FVector3f(0.5f, 0.5f, 0.5f);
        FStaticMeshSection section{};
        section.FirstIndex   = 0U;
        section.IndexCount   = 3U;
        section.BaseVertex   = 0;
        section.MaterialSlot = materialSlot;
        lod.mSections.PushBack(section);
        mesh.mBounds = lod.mBounds;
        return mesh;
    }

    auto MakeMaterialComponent(const FAssetHandle& handle) -> FMeshMaterialComponent {
        FMeshMaterialComponent component{};
        component.SetMaterialTemplate(0U, handle);
        return component;
    }

    auto MakeSceneEntry(const FStaticMeshData& mesh, const FMeshMaterialComponent& materials,
        const AltinaEngine::Core::Math::FVector3f& translation =
            AltinaEngine::Core::Math::FVector3f(0.0f)) -> FSceneStaticMesh {
        FSceneStaticMesh entry{};
        entry.Mesh              = &mesh;
        entry.Materials         = &materials;
        entry.WorldMatrix       = AltinaEngine::Core::Math::LinAlg::Identity<f32, 4U>();
        entry.WorldMatrix(0, 3) = translation[0];
        entry.WorldMatrix(1, 3) = translation[1];
        entry.WorldMatrix(2, 3) = translation[2];
        entry.PrevWorldMatrix   = entry.WorldMatrix;
        return entry;
    }

    auto MakeView(const AltinaEngine::Core::Math::FVector3f& translation =
                      AltinaEngine::Core::Math::FVector3f(0.0f)) -> FSceneView {
        FSceneView view{};
        view.View.ViewRect                      = FViewRect{ 0, 0, 1280U, 720U };
        view.View.RenderTargetExtent            = FRenderTargetExtent2D{ 1280U, 720U };
        view.View.Camera.mNearPlane             = 0.1f;
        view.View.Camera.mFarPlane              = 100.0f;
        view.View.Camera.mTransform.Translation = translation;
        view.View.BeginFrame();
        return view;
    }
} // namespace

TEST_CASE("GameScene.SceneBatching.GroupsDifferentGeometryIntoSameMaterialBucket") {
    FMaterialConverterGuard converterGuard(
        [](const FAssetHandle&, const AltinaEngine::Asset::FMeshMaterialParameterBlock&) {
            return FMaterial{};
        });

    FStaticMeshData        meshA          = MakeStaticMesh();
    FStaticMeshData        meshB          = MakeStaticMesh();
    const FAssetHandle     materialHandle = MakeMaterialHandle(1U);
    FMeshMaterialComponent materials      = MakeMaterialComponent(materialHandle);

    FRenderScene           scene{};
    scene.StaticMeshes.PushBack(MakeSceneEntry(meshA, materials));
    scene.StaticMeshes.PushBack(MakeSceneEntry(meshB, materials));

    FMaterialCache         materialCache{};
    FSceneBatchBuilder     builder{};
    FSceneBatchBuildParams params{};
    params.Pass             = EMaterialPass::BasePass;
    params.bAllowInstancing = true;

    AltinaEngine::RenderCore::Render::FDrawList drawList{};
    builder.Build(scene, FSceneView{}, params, materialCache, drawList);

    REQUIRE_EQ(drawList.GetBucketCount(), 1U);
    REQUIRE_EQ(drawList.GetBatchCount(), 2U);
    REQUIRE_EQ(drawList.mBuckets.Size(), 1U);
    REQUIRE_EQ(drawList.mBuckets[0].mBatches.Size(), 2U);
    REQUIRE(drawList.mBuckets[0].mMaterial != nullptr);
    REQUIRE(drawList.mBuckets[0].mMaterial == drawList.mBuckets[0].mBatches[0].mMaterial);
    REQUIRE(drawList.mBuckets[0].mMaterial == drawList.mBuckets[0].mBatches[1].mMaterial);
}

TEST_CASE("GameScene.SceneBatching.InstancesMatchingGeometryWithinMaterialBucket") {
    FMaterialConverterGuard converterGuard(
        [](const FAssetHandle&, const AltinaEngine::Asset::FMeshMaterialParameterBlock&) {
            return FMaterial{};
        });

    FStaticMeshData        mesh           = MakeStaticMesh();
    const FAssetHandle     materialHandle = MakeMaterialHandle(2U);
    FMeshMaterialComponent materials      = MakeMaterialComponent(materialHandle);

    FRenderScene           scene{};
    scene.StaticMeshes.PushBack(MakeSceneEntry(mesh, materials));
    scene.StaticMeshes.PushBack(MakeSceneEntry(mesh, materials));

    FMaterialCache         materialCache{};
    FSceneBatchBuilder     builder{};
    FSceneBatchBuildParams params{};
    params.Pass             = EMaterialPass::BasePass;
    params.bAllowInstancing = true;

    AltinaEngine::RenderCore::Render::FDrawList drawList{};
    builder.Build(scene, FSceneView{}, params, materialCache, drawList);

    REQUIRE_EQ(drawList.GetBucketCount(), 1U);
    REQUIRE_EQ(drawList.GetBatchCount(), 1U);
    REQUIRE_EQ(drawList.mBuckets.Size(), 1U);
    REQUIRE_EQ(drawList.mBuckets[0].mBatches.Size(), 1U);
    REQUIRE_EQ(drawList.mBuckets[0].mBatches[0].mInstances.Size(), 2U);
}

TEST_CASE("GameScene.SceneBatching.KeepsMaterialBucketWhenInstancingDisabled") {
    FMaterialConverterGuard converterGuard(
        [](const FAssetHandle&, const AltinaEngine::Asset::FMeshMaterialParameterBlock&) {
            return FMaterial{};
        });

    FStaticMeshData        mesh           = MakeStaticMesh();
    const FAssetHandle     materialHandle = MakeMaterialHandle(3U);
    FMeshMaterialComponent materials      = MakeMaterialComponent(materialHandle);

    FRenderScene           scene{};
    scene.StaticMeshes.PushBack(MakeSceneEntry(mesh, materials));
    scene.StaticMeshes.PushBack(MakeSceneEntry(mesh, materials));

    FMaterialCache         materialCache{};
    FSceneBatchBuilder     builder{};
    FSceneBatchBuildParams params{};
    params.Pass             = EMaterialPass::BasePass;
    params.bAllowInstancing = false;

    AltinaEngine::RenderCore::Render::FDrawList drawList{};
    builder.Build(scene, FSceneView{}, params, materialCache, drawList);

    REQUIRE_EQ(drawList.GetBucketCount(), 1U);
    REQUIRE_EQ(drawList.GetBatchCount(), 2U);
    REQUIRE_EQ(drawList.mBuckets.Size(), 1U);
    REQUIRE_EQ(drawList.mBuckets[0].mBatches.Size(), 2U);
    REQUIRE_EQ(drawList.mBuckets[0].mBatches[0].mInstances.Size(), 1U);
    REQUIRE_EQ(drawList.mBuckets[0].mBatches[1].mInstances.Size(), 1U);
}

TEST_CASE("GameScene.SceneBatching.FrustumCullsInvisibleStaticMeshes") {
    FMaterialConverterGuard converterGuard(
        [](const FAssetHandle&, const AltinaEngine::Asset::FMeshMaterialParameterBlock&) {
            return FMaterial{};
        });

    FStaticMeshData        mesh           = MakeStaticMesh();
    const FAssetHandle     materialHandle = MakeMaterialHandle(4U);
    FMeshMaterialComponent materials      = MakeMaterialComponent(materialHandle);

    FRenderScene           scene{};
    scene.StaticMeshes.PushBack(
        MakeSceneEntry(mesh, materials, AltinaEngine::Core::Math::FVector3f(0.0f, 0.0f, 5.0f)));
    scene.StaticMeshes.PushBack(
        MakeSceneEntry(mesh, materials, AltinaEngine::Core::Math::FVector3f(10.0f, 0.0f, 5.0f)));

    FMaterialCache         materialCache{};
    FSceneBatchBuilder     builder{};
    FSceneBatchBuildParams params{};
    params.Pass                  = EMaterialPass::BasePass;
    params.bAllowInstancing      = true;
    params.bEnableFrustumCulling = true;

    AltinaEngine::RenderCore::Render::FDrawList drawList{};
    const FSceneView                            view = MakeView();
    builder.Build(scene, view, params, materialCache, drawList);

    REQUIRE_EQ(drawList.GetBucketCount(), 1U);
    REQUIRE_EQ(drawList.GetBatchCount(), 1U);
    REQUIRE_EQ(drawList.mBuckets[0].mBatches[0].mInstances.Size(), 1U);
}

TEST_CASE("GameScene.SceneBatching.CanDisableFrustumCulling") {
    FMaterialConverterGuard converterGuard(
        [](const FAssetHandle&, const AltinaEngine::Asset::FMeshMaterialParameterBlock&) {
            return FMaterial{};
        });

    FStaticMeshData        mesh           = MakeStaticMesh();
    const FAssetHandle     materialHandle = MakeMaterialHandle(5U);
    FMeshMaterialComponent materials      = MakeMaterialComponent(materialHandle);

    FRenderScene           scene{};
    scene.StaticMeshes.PushBack(
        MakeSceneEntry(mesh, materials, AltinaEngine::Core::Math::FVector3f(0.0f, 0.0f, 5.0f)));
    scene.StaticMeshes.PushBack(
        MakeSceneEntry(mesh, materials, AltinaEngine::Core::Math::FVector3f(10.0f, 0.0f, 5.0f)));

    FMaterialCache         materialCache{};
    FSceneBatchBuilder     builder{};
    FSceneBatchBuildParams params{};
    params.Pass                  = EMaterialPass::BasePass;
    params.bAllowInstancing      = true;
    params.bEnableFrustumCulling = false;

    AltinaEngine::RenderCore::Render::FDrawList drawList{};
    const FSceneView                            view = MakeView();
    builder.Build(scene, view, params, materialCache, drawList);

    REQUIRE_EQ(drawList.GetBucketCount(), 1U);
    REQUIRE_EQ(drawList.GetBatchCount(), 1U);
    REQUIRE_EQ(drawList.mBuckets[0].mBatches[0].mInstances.Size(), 2U);
}

TEST_CASE("GameScene.SceneBatching.FrustumCullsMeshesOutsideDepthRange") {
    FMaterialConverterGuard converterGuard(
        [](const FAssetHandle&, const AltinaEngine::Asset::FMeshMaterialParameterBlock&) {
            return FMaterial{};
        });

    FStaticMeshData mesh      = MakeStaticMesh();
    mesh.mLods[0].mBounds.Min = AltinaEngine::Core::Math::FVector3f(-0.01f, -0.01f, -0.01f);
    mesh.mLods[0].mBounds.Max = AltinaEngine::Core::Math::FVector3f(0.01f, 0.01f, 0.01f);
    mesh.mBounds              = mesh.mLods[0].mBounds;
    const FAssetHandle     materialHandle = MakeMaterialHandle(6U);
    FMeshMaterialComponent materials      = MakeMaterialComponent(materialHandle);

    FRenderScene           scene{};
    scene.StaticMeshes.PushBack(
        MakeSceneEntry(mesh, materials, AltinaEngine::Core::Math::FVector3f(0.0f, 0.0f, 5.0f)));
    scene.StaticMeshes.PushBack(
        MakeSceneEntry(mesh, materials, AltinaEngine::Core::Math::FVector3f(0.0f, 0.0f, 0.01f)));
    scene.StaticMeshes.PushBack(
        MakeSceneEntry(mesh, materials, AltinaEngine::Core::Math::FVector3f(0.0f, 0.0f, 20000.0f)));

    FMaterialCache         materialCache{};
    FSceneBatchBuilder     builder{};
    FSceneBatchBuildParams params{};
    params.Pass                  = EMaterialPass::BasePass;
    params.bAllowInstancing      = true;
    params.bEnableFrustumCulling = true;

    AltinaEngine::RenderCore::Render::FDrawList drawList{};
    const FSceneView                            view = MakeView();
    builder.Build(scene, view, params, materialCache, drawList);

    REQUIRE_EQ(drawList.GetBucketCount(), 1U);
    REQUIRE_EQ(drawList.GetBatchCount(), 1U);
    REQUIRE_EQ(drawList.mBuckets[0].mBatches[0].mInstances.Size(), 1U);
}

TEST_CASE("GameScene.SceneBatching.FrustumCullsMeshesBehindCamera") {
    FMaterialConverterGuard converterGuard(
        [](const FAssetHandle&, const AltinaEngine::Asset::FMeshMaterialParameterBlock&) {
            return FMaterial{};
        });

    FStaticMeshData        mesh           = MakeStaticMesh();
    const FAssetHandle     materialHandle = MakeMaterialHandle(7U);
    FMeshMaterialComponent materials      = MakeMaterialComponent(materialHandle);

    FRenderScene           scene{};
    scene.StaticMeshes.PushBack(
        MakeSceneEntry(mesh, materials, AltinaEngine::Core::Math::FVector3f(0.0f, 0.0f, 5.0f)));
    scene.StaticMeshes.PushBack(
        MakeSceneEntry(mesh, materials, AltinaEngine::Core::Math::FVector3f(0.0f, 0.0f, -1.0f)));

    FMaterialCache         materialCache{};
    FSceneBatchBuilder     builder{};
    FSceneBatchBuildParams params{};
    params.Pass                  = EMaterialPass::BasePass;
    params.bAllowInstancing      = true;
    params.bEnableFrustumCulling = true;

    AltinaEngine::RenderCore::Render::FDrawList drawList{};
    const FSceneView                            view = MakeView();
    builder.Build(scene, view, params, materialCache, drawList);

    REQUIRE_EQ(drawList.GetBucketCount(), 1U);
    REQUIRE_EQ(drawList.GetBatchCount(), 1U);
    REQUIRE_EQ(drawList.mBuckets[0].mBatches[0].mInstances.Size(), 1U);
}

TEST_CASE("GameScene.SceneBatching.CustomCullMatrixOverridesViewFrustum") {
    FMaterialConverterGuard converterGuard(
        [](const FAssetHandle&, const AltinaEngine::Asset::FMeshMaterialParameterBlock&) {
            return FMaterial{};
        });

    FStaticMeshData        mesh           = MakeStaticMesh();
    const FAssetHandle     materialHandle = MakeMaterialHandle(8U);
    FMeshMaterialComponent materials      = MakeMaterialComponent(materialHandle);

    FRenderScene           scene{};
    scene.StaticMeshes.PushBack(
        MakeSceneEntry(mesh, materials, AltinaEngine::Core::Math::FVector3f(0.0f, 0.0f, 5.0f)));
    scene.StaticMeshes.PushBack(
        MakeSceneEntry(mesh, materials, AltinaEngine::Core::Math::FVector3f(10.0f, 0.0f, 5.0f)));

    FMaterialCache         materialCache{};
    FSceneBatchBuilder     builder{};
    FSceneBatchBuildParams params{};
    params.Pass                  = EMaterialPass::BasePass;
    params.bAllowInstancing      = true;
    params.bEnableFrustumCulling = true;
    params.bUseCustomCullMatrix  = true;

    const FSceneView defaultView = MakeView();
    const FSceneView cullView    = MakeView(AltinaEngine::Core::Math::FVector3f(10.0f, 0.0f, 0.0f));
    params.mCullViewProj         = cullView.View.Matrices.ViewProj;

    AltinaEngine::RenderCore::Render::FDrawList drawList{};
    builder.Build(scene, defaultView, params, materialCache, drawList);

    REQUIRE_EQ(drawList.GetBucketCount(), 1U);
    REQUIRE_EQ(drawList.GetBatchCount(), 1U);
    REQUIRE_EQ(drawList.mBuckets[0].mBatches[0].mInstances.Size(), 1U);
    REQUIRE_EQ(drawList.mBuckets[0].mBatches[0].mInstances[0].mWorld(0, 3), 10.0f);
}

TEST_CASE("GameScene.SceneBatching.ShadowDistanceCullsFarCasters") {
    FMaterialConverterGuard converterGuard(
        [](const FAssetHandle&, const AltinaEngine::Asset::FMeshMaterialParameterBlock&) {
            return FMaterial{};
        });

    FStaticMeshData        mesh           = MakeStaticMesh();
    const FAssetHandle     materialHandle = MakeMaterialHandle(9U);
    FMeshMaterialComponent materials      = MakeMaterialComponent(materialHandle);

    FRenderScene           scene{};
    scene.StaticMeshes.PushBack(
        MakeSceneEntry(mesh, materials, AltinaEngine::Core::Math::FVector3f(0.0f, 0.0f, 5.0f)));
    scene.StaticMeshes.PushBack(
        MakeSceneEntry(mesh, materials, AltinaEngine::Core::Math::FVector3f(0.0f, 0.0f, 25.0f)));

    FMaterialCache         materialCache{};
    FSceneBatchBuilder     builder{};
    FSceneBatchBuildParams params{};
    params.Pass                         = EMaterialPass::ShadowPass;
    params.bAllowInstancing             = true;
    params.bEnableFrustumCulling        = false;
    params.bEnableShadowDistanceCulling = true;
    params.mShadowCullMaxViewDepth      = 10.0f;
    params.mShadowCullViewDepthPadding  = 0.0f;

    AltinaEngine::RenderCore::Render::FDrawList drawList{};
    const FSceneView                            view = MakeView();
    builder.Build(scene, view, params, materialCache, drawList);

    REQUIRE_EQ(drawList.GetBucketCount(), 1U);
    REQUIRE_EQ(drawList.GetBatchCount(), 1U);
    REQUIRE_EQ(drawList.mBuckets[0].mBatches[0].mInstances.Size(), 1U);
    REQUIRE_EQ(drawList.mBuckets[0].mBatches[0].mInstances[0].mWorld(2, 3), 5.0f);
}

TEST_CASE("GameScene.SceneBatching.SmallCasterCullsTinyShadowCasters") {
    FMaterialConverterGuard converterGuard(
        [](const FAssetHandle&, const AltinaEngine::Asset::FMeshMaterialParameterBlock&) {
            return FMaterial{};
        });

    FStaticMeshData largeMesh      = MakeStaticMesh();
    FStaticMeshData smallMesh      = MakeStaticMesh();
    smallMesh.mLods[0].mBounds.Min = AltinaEngine::Core::Math::FVector3f(-0.01f, -0.01f, -0.01f);
    smallMesh.mLods[0].mBounds.Max = AltinaEngine::Core::Math::FVector3f(0.01f, 0.01f, 0.01f);
    smallMesh.mBounds              = smallMesh.mLods[0].mBounds;

    const FAssetHandle     materialHandle = MakeMaterialHandle(10U);
    FMeshMaterialComponent materials      = MakeMaterialComponent(materialHandle);

    FRenderScene           scene{};
    scene.StaticMeshes.PushBack(MakeSceneEntry(
        largeMesh, materials, AltinaEngine::Core::Math::FVector3f(0.0f, 0.0f, 5.0f)));
    scene.StaticMeshes.PushBack(MakeSceneEntry(
        smallMesh, materials, AltinaEngine::Core::Math::FVector3f(1.0f, 0.0f, 5.0f)));

    FMaterialCache         materialCache{};
    FSceneBatchBuilder     builder{};
    FSceneBatchBuildParams params{};
    params.Pass                            = EMaterialPass::ShadowPass;
    params.bAllowInstancing                = true;
    params.bEnableFrustumCulling           = false;
    params.bEnableShadowSmallCasterCulling = true;
    params.mShadowMinCasterRadiusWs        = 0.1f;

    AltinaEngine::RenderCore::Render::FDrawList drawList{};
    const FSceneView                            view = MakeView();
    builder.Build(scene, view, params, materialCache, drawList);

    REQUIRE_EQ(drawList.GetBucketCount(), 1U);
    REQUIRE_EQ(drawList.GetBatchCount(), 1U);
    REQUIRE_EQ(drawList.mBuckets[0].mBatches[0].mInstances.Size(), 1U);
    REQUIRE_EQ(drawList.mBuckets[0].mBatches[0].mInstances[0].mWorld(0, 3), 0.0f);
}
