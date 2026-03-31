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
        FStaticMeshSection section{};
        section.FirstIndex   = 0U;
        section.IndexCount   = 3U;
        section.BaseVertex   = 0;
        section.MaterialSlot = materialSlot;
        lod.mSections.PushBack(section);
        return mesh;
    }

    auto MakeMaterialComponent(const FAssetHandle& handle) -> FMeshMaterialComponent {
        FMeshMaterialComponent component{};
        component.SetMaterialTemplate(0U, handle);
        return component;
    }

    auto MakeSceneEntry(const FStaticMeshData& mesh, const FMeshMaterialComponent& materials)
        -> FSceneStaticMesh {
        FSceneStaticMesh entry{};
        entry.Mesh            = &mesh;
        entry.Materials       = &materials;
        entry.WorldMatrix     = AltinaEngine::Core::Math::LinAlg::Identity<f32, 4U>();
        entry.PrevWorldMatrix = entry.WorldMatrix;
        return entry;
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
