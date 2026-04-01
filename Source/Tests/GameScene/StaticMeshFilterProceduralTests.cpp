#include "TestHarness.h"

#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/Runtime/MaterialCache.h"
#include "Engine/Runtime/SceneBatching.h"

namespace {
    namespace Container = AltinaEngine::Core::Container;
    namespace Engine    = AltinaEngine::Engine;
    namespace Geometry  = AltinaEngine::RenderCore::Geometry;
    namespace Math      = AltinaEngine::Core::Math;
    using AltinaEngine::Move;
    using AltinaEngine::u32;
    using AltinaEngine::u64;
    using AltinaEngine::GameScene::FStaticMeshFilterComponent;

    auto MakeTinyTriangleMesh() -> Geometry::FStaticMeshData {
        Geometry::FStaticMeshData mesh{};
        mesh.mLods.Reserve(1);

        Geometry::FStaticMeshLodData lod{};
        lod.mPrimitiveTopology = AltinaEngine::Rhi::ERhiPrimitiveTopology::TriangleList;

        const Math::FVector3f pos[] = {
            Math::FVector3f(0.0f, 0.0f, 0.0f),
            Math::FVector3f(1.0f, 0.0f, 0.0f),
            Math::FVector3f(0.0f, 0.0f, 1.0f),
        };
        const Math::FVector4f tan[] = {
            Math::FVector4f(1.0f, 0.0f, 0.0f, 1.0f),
            Math::FVector4f(1.0f, 0.0f, 0.0f, 1.0f),
            Math::FVector4f(1.0f, 0.0f, 0.0f, 1.0f),
        };
        const Math::FVector2f uv[] = {
            Math::FVector2f(0.0f, 0.0f),
            Math::FVector2f(1.0f, 0.0f),
            Math::FVector2f(0.0f, 1.0f),
        };
        const u32 indices[] = { 0U, 1U, 2U };

        lod.SetPositions(pos, 3U);
        lod.SetTangents(tan, 3U);
        lod.SetUV0(uv, 3U);
        lod.SetUV1(uv, 3U);
        lod.SetIndices(indices, 3U, AltinaEngine::Rhi::ERhiIndexType::Uint32);

        Geometry::FStaticMeshSection section{};
        section.FirstIndex   = 0U;
        section.IndexCount   = 3U;
        section.BaseVertex   = 0;
        section.MaterialSlot = 0U;
        lod.mSections.PushBack(section);

        lod.mBounds.Min = pos[0];
        lod.mBounds.Max = pos[0];
        for (u32 i = 1U; i < 3U; ++i) {
            for (u32 c = 0U; c < 3U; ++c) {
                if (pos[i][c] < lod.mBounds.Min[c])
                    lod.mBounds.Min[c] = pos[i][c];
                if (pos[i][c] > lod.mBounds.Max[c])
                    lod.mBounds.Max[c] = pos[i][c];
            }
        }

        mesh.mLods.PushBack(Move(lod));
        mesh.mBounds = mesh.mLods[0].mBounds;
        return mesh;
    }

    auto MakeIdentityMatrix() -> Math::FMatrix4x4f {
        Math::FMatrix4x4f matrix(0.0f);
        for (u32 i = 0U; i < 4U; ++i) {
            matrix(i, i) = 1.0f;
        }
        return matrix;
    }

    auto MakeSharedMeshEntry(const AltinaEngine::Asset::FAssetHandle& handle, u64 geometryKey)
        -> Container::TShared<Engine::FStaticMeshCacheEntry> {
        auto entry          = Container::MakeShared<Engine::FStaticMeshCacheEntry>();
        entry->mHandle      = handle;
        entry->mGeometryKey = geometryKey;
        entry->mMesh        = MakeTinyTriangleMesh();
        return entry;
    }

    auto MakeUuid(u8 b0, u8 b1, u8 b2, u8 b3, u8 b4, u8 b5, u8 b6, u8 b7, u8 b8, u8 b9, u8 b10,
        u8 b11, u8 b12, u8 b13, u8 b14, u8 b15) -> AltinaEngine::FUuid {
        AltinaEngine::FUuid::FBytes bytes{};
        bytes[0]  = b0;
        bytes[1]  = b1;
        bytes[2]  = b2;
        bytes[3]  = b3;
        bytes[4]  = b4;
        bytes[5]  = b5;
        bytes[6]  = b6;
        bytes[7]  = b7;
        bytes[8]  = b8;
        bytes[9]  = b9;
        bytes[10] = b10;
        bytes[11] = b11;
        bytes[12] = b12;
        bytes[13] = b13;
        bytes[14] = b14;
        bytes[15] = b15;
        return AltinaEngine::FUuid(bytes);
    }
} // namespace

TEST_CASE("GameScene.StaticMeshFilterComponent.ProceduralOverrideStable") {
    FStaticMeshFilterComponent comp{};
    auto                       mesh = MakeTinyTriangleMesh();
    comp.SetStaticMeshData(Move(mesh));

    const auto& m0 = comp.GetStaticMesh();
    REQUIRE(m0.IsValid());
    REQUIRE_EQ(m0.GetLodCount(), 1U);
    const u32 v0 = m0.mLods[0].GetVertexCount();
    const u32 i0 = m0.mLods[0].GetIndexCount();
    REQUIRE_EQ(v0, 3U);
    REQUIRE_EQ(i0, 3U);

    const auto& m1 = comp.GetStaticMesh();
    REQUIRE(m1.IsValid());
    REQUIRE_EQ(m1.mLods[0].GetVertexCount(), v0);
    REQUIRE_EQ(m1.mLods[0].GetIndexCount(), i0);
}

TEST_CASE("GameScene.StaticMeshFilterComponent.ProceduralOverrideBeatsAssetResolve") {
    const auto old = FStaticMeshFilterComponent::AssetToStaticMeshConverter;
    FStaticMeshFilterComponent::AssetToStaticMeshConverter =
        [](const AltinaEngine::Asset::FAssetHandle& handle) {
            return MakeSharedMeshEntry(handle, 101U);
        };

    FStaticMeshFilterComponent comp{};
    comp.SetStaticMeshData(MakeTinyTriangleMesh());

    // Even with an asset converter installed, procedural override should keep the mesh intact.
    const auto& mesh = comp.GetStaticMesh();
    REQUIRE(mesh.IsValid());
    REQUIRE_EQ(mesh.mLods[0].GetVertexCount(), 3U);
    REQUIRE_EQ(mesh.mLods[0].GetIndexCount(), 3U);

    FStaticMeshFilterComponent::AssetToStaticMeshConverter = old;
}

TEST_CASE("GameScene.StaticMeshFilterComponent.SetStaticMeshAssetDisablesProceduralOverride") {
    FStaticMeshFilterComponent comp{};
    comp.SetStaticMeshData(MakeTinyTriangleMesh());
    REQUIRE(comp.GetStaticMesh().IsValid());

    comp.SetStaticMeshAsset({});
    // No converter => mesh stays empty.
    REQUIRE(!comp.GetStaticMesh().IsValid());
}

TEST_CASE("GameScene.StaticMeshFilterComponent.SharedAssetResolveReusesMeshEntry") {
    const auto old = FStaticMeshFilterComponent::AssetToStaticMeshConverter;

    const AltinaEngine::Asset::FAssetHandle sharedHandle{
        MakeUuid(0x10U, 0x20U, 0x30U, 0x40U, 0x50U, 0x60U, 0x70U, 0x80U, 0x90U, 0xA0U, 0xB0U, 0xC0U,
            0xD0U, 0xE0U, 0xF0U, 0x01U),
        AltinaEngine::Asset::EAssetType::Mesh
    };
    const auto sharedEntry = MakeSharedMeshEntry(sharedHandle, 0xA55AULL);

    FStaticMeshFilterComponent::AssetToStaticMeshConverter =
        [sharedEntry](const AltinaEngine::Asset::FAssetHandle& handle) {
            return (handle == sharedEntry->mHandle)
                ? sharedEntry
                : Container::TShared<Engine::FStaticMeshCacheEntry>{};
        };

    FStaticMeshFilterComponent compA{};
    FStaticMeshFilterComponent compB{};
    compA.SetStaticMeshAsset(sharedHandle);
    compB.SetStaticMeshAsset(sharedHandle);

    const auto& meshA = compA.GetStaticMesh();
    const auto& meshB = compB.GetStaticMesh();
    REQUIRE(meshA.IsValid());
    REQUIRE(meshB.IsValid());
    REQUIRE(&meshA == &meshB);
    REQUIRE_EQ(compA.GetStaticMeshGeometryKey(), 0xA55AULL);
    REQUIRE_EQ(compB.GetStaticMeshGeometryKey(), 0xA55AULL);

    FStaticMeshFilterComponent::AssetToStaticMeshConverter = old;
}

TEST_CASE("Engine.SceneBatchBuilder.InstancingUsesSharedMeshGeometryKey") {
    const AltinaEngine::Asset::FAssetHandle sharedHandle{
        MakeUuid(0x01U, 0x23U, 0x45U, 0x67U, 0x89U, 0xABU, 0xCDU, 0xEFU, 0x10U, 0x32U, 0x54U, 0x76U,
            0x98U, 0xBAU, 0xDCU, 0xFEU),
        AltinaEngine::Asset::EAssetType::Mesh
    };
    const auto               sharedEntry = MakeSharedMeshEntry(sharedHandle, 0xBEEF1234ULL);

    Engine::FRenderScene     scene{};
    Engine::FSceneStaticMesh entryA{};
    entryA.MeshGeometryKey = sharedEntry->mGeometryKey;
    entryA.Mesh            = &sharedEntry->mMesh;
    entryA.WorldMatrix     = MakeIdentityMatrix();
    entryA.PrevWorldMatrix = entryA.WorldMatrix;

    Engine::FSceneStaticMesh entryB = entryA;
    scene.StaticMeshes.PushBack(entryA);
    scene.StaticMeshes.PushBack(entryB);

    Engine::FSceneView             view{};
    Engine::FSceneBatchBuildParams params{};
    params.bEnableFrustumCulling = false;
    params.bAllowInstancing      = true;

    Engine::FMaterialCache                      materialCache{};
    AltinaEngine::RenderCore::Render::FDrawList drawList{};
    Engine::FSceneBatchBuilder                  builder{};
    builder.Build(scene, view, params, materialCache, drawList);

    REQUIRE_EQ(drawList.GetBucketCount(), 1U);
    REQUIRE_EQ(drawList.GetBatchCount(), 1U);

    u32 batchCount    = 0U;
    u32 instanceCount = 0U;
    drawList.ForEachBatch([&](const auto& batch) {
        ++batchCount;
        instanceCount += static_cast<u32>(batch.mInstances.Size());
    });
    REQUIRE_EQ(batchCount, 1U);
    REQUIRE_EQ(instanceCount, 2U);
}
