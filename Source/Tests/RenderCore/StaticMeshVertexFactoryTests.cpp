#include "TestHarness.h"

#include "Geometry/StaticMeshVertexFactory.h"
#include "Geometry/VertexLayoutBuilder.h"

namespace {
    using AltinaEngine::RenderCore::Geometry::BuildStaticMeshLegacyVertexLayout;
    using AltinaEngine::RenderCore::Geometry::BuildStaticMeshProvidedLayout;
    using AltinaEngine::RenderCore::Geometry::FVertexFactoryProvidedLayout;
    using AltinaEngine::RenderCore::Geometry::MakeVertexSemanticKey;
    using AltinaEngine::Rhi::ERhiFormat;
    using AltinaEngine::Rhi::FRhiVertexLayoutDesc;
} // namespace

TEST_CASE("RenderCore.StaticMeshVertexFactory.ProvidedLayout") {
    FVertexFactoryProvidedLayout provided{};
    REQUIRE(BuildStaticMeshProvidedLayout(provided));
    REQUIRE_EQ(provided.mElements.Size(), 3U);

    const auto& position = provided.mElements[0];
    REQUIRE(position.mSemantic == MakeVertexSemanticKey(TEXT("POSITION"), 0U));
    REQUIRE(position.mFormat == ERhiFormat::R32G32B32Float);
    REQUIRE_EQ(position.mInputSlot, 0U);
    REQUIRE_EQ(position.mAlignedByteOffset, 0U);

    const auto& normal = provided.mElements[1];
    REQUIRE(normal.mSemantic == MakeVertexSemanticKey(TEXT("NORMAL"), 0U));
    REQUIRE(normal.mFormat == ERhiFormat::R32G32B32Float);
    REQUIRE_EQ(normal.mInputSlot, 1U);
    REQUIRE_EQ(normal.mAlignedByteOffset, 0U);

    const auto& uv0 = provided.mElements[2];
    REQUIRE(uv0.mSemantic == MakeVertexSemanticKey(TEXT("TEXCOORD"), 0U));
    REQUIRE(uv0.mFormat == ERhiFormat::R32G32Float);
    REQUIRE_EQ(uv0.mInputSlot, 2U);
    REQUIRE_EQ(uv0.mAlignedByteOffset, 0U);
}

TEST_CASE("RenderCore.StaticMeshVertexFactory.LegacyVertexLayout") {
    FRhiVertexLayoutDesc layout{};
    REQUIRE(BuildStaticMeshLegacyVertexLayout(layout));
    REQUIRE_EQ(layout.mAttributes.Size(), 3U);

    REQUIRE(layout.mAttributes[0].mSemanticName == TEXT("POSITION"));
    REQUIRE_EQ(layout.mAttributes[0].mInputSlot, 0U);
    REQUIRE(layout.mAttributes[1].mSemanticName == TEXT("NORMAL"));
    REQUIRE_EQ(layout.mAttributes[1].mInputSlot, 1U);
    REQUIRE(layout.mAttributes[2].mSemanticName == TEXT("TEXCOORD"));
    REQUIRE_EQ(layout.mAttributes[2].mInputSlot, 2U);
}
