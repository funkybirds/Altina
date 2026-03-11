#include "TestHarness.h"

#include "Engine/GameScene/StaticMeshFilterComponent.h"

namespace {
    namespace Geometry = AltinaEngine::RenderCore::Geometry;
    namespace Math     = AltinaEngine::Core::Math;
    using AltinaEngine::Move;
    using AltinaEngine::u32;
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
        [](const AltinaEngine::Asset::FAssetHandle&) { return MakeTinyTriangleMesh(); };

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
