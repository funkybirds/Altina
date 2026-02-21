#include "RenderAsset/MeshAssetConversion.h"

#include "Asset/AssetBinary.h"
#include "Math/Vector.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Types/Traits.h"

namespace AltinaEngine::Rendering {
    namespace {
        using Asset::FMeshVertexAttributeDesc;

        struct FAttributeView {
            bool                     Valid = false;
            FMeshVertexAttributeDesc Desc{};
        };

        [[nodiscard]] auto FindAttribute(
            const Core::Container::TVector<FMeshVertexAttributeDesc>& attributes, u32 semantic,
            u32 semanticIndex) -> FAttributeView {
            for (const auto& attr : attributes) {
                if (attr.Semantic == semantic && attr.SemanticIndex == semanticIndex) {
                    return { true, attr };
                }
            }
            return {};
        }

        [[nodiscard]] auto FindAttributeAnyIndex(
            const Core::Container::TVector<FMeshVertexAttributeDesc>& attributes, u32 semantic)
            -> FAttributeView {
            for (const auto& attr : attributes) {
                if (attr.Semantic == semantic) {
                    return { true, attr };
                }
            }
            return {};
        }

        [[nodiscard]] auto ReadFloats(const u8* base, u32 format, f32* out, u32 outCount) -> bool {
            if (base == nullptr || out == nullptr || outCount == 0U) {
                return false;
            }

            u32 formatCount = 0U;
            switch (format) {
                case Asset::kMeshVertexFormatR32Float: {
                    formatCount = 1U;
                    break;
                }
                case Asset::kMeshVertexFormatR32G32Float: {
                    formatCount = 2U;
                    break;
                }
                case Asset::kMeshVertexFormatR32G32B32Float: {
                    formatCount = 3U;
                    break;
                }
                case Asset::kMeshVertexFormatR32G32B32A32Float: {
                    formatCount = 4U;
                    break;
                }
                default:
                    return false;
            }

            f32 tmp[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            Core::Platform::Generic::Memcpy(tmp, base, sizeof(f32) * formatCount);
            for (u32 i = 0U; i < outCount; ++i) {
                out[i] = (i < formatCount) ? tmp[i] : 0.0f;
            }
            return true;
        }

        [[nodiscard]] constexpr auto GetFormatSizeBytes(u32 format) -> u32 {
            switch (format) {
                case Asset::kMeshVertexFormatR32Float:
                    return 4U;
                case Asset::kMeshVertexFormatR32G32Float:
                    return 8U;
                case Asset::kMeshVertexFormatR32G32B32Float:
                    return 12U;
                case Asset::kMeshVertexFormatR32G32B32A32Float:
                    return 16U;
                default:
                    return 0U;
            }
        }

        [[nodiscard]] auto ResolveIndexType(u32 indexType, Rhi::ERhiIndexType& out) -> bool {
            switch (indexType) {
                case Asset::kMeshIndexTypeUint16:
                    out = Rhi::ERhiIndexType::Uint16;
                    return true;
                case Asset::kMeshIndexTypeUint32:
                    out = Rhi::ERhiIndexType::Uint32;
                    return true;
                default:
                    return false;
            }
        }
    } // namespace

    auto ConvertMeshAssetToStaticMesh(
        const Asset::FMeshAsset& asset, RenderCore::Geometry::FStaticMeshData& outMesh) -> bool {
        const auto& desc       = asset.GetDesc();
        const auto& attributes = asset.GetAttributes();
        const auto& subMeshes  = asset.GetSubMeshes();
        const auto& vertexData = asset.GetVertexData();
        const auto& indexData  = asset.GetIndexData();

        if (desc.VertexCount == 0U || desc.IndexCount == 0U || desc.VertexStride == 0U) {
            return false;
        }

        const u64 expectedVertexBytes =
            static_cast<u64>(desc.VertexCount) * static_cast<u64>(desc.VertexStride);
        if (vertexData.Size() < static_cast<usize>(expectedVertexBytes)) {
            return false;
        }

        Rhi::ERhiIndexType indexType = Rhi::ERhiIndexType::Uint16;
        if (!ResolveIndexType(desc.IndexType, indexType)) {
            return false;
        }

        const u32 indexStride = RenderCore::Geometry::FStaticMeshLodData::GetIndexStrideBytes(
            indexType);
        const u64 expectedIndexBytes =
            static_cast<u64>(desc.IndexCount) * static_cast<u64>(indexStride);
        if (indexData.Size() < static_cast<usize>(expectedIndexBytes)) {
            return false;
        }

        auto positionAttr = FindAttribute(attributes, Asset::kMeshSemanticPosition, 0U);
        if (!positionAttr.Valid) {
            positionAttr = FindAttributeAnyIndex(attributes, Asset::kMeshSemanticPosition);
        }
        if (!positionAttr.Valid) {
            return false;
        }

        auto normalAttr  = FindAttribute(attributes, Asset::kMeshSemanticNormal, 0U);
        auto tangentAttr = FindAttribute(attributes, Asset::kMeshSemanticTangent, 0U);
        auto uv0Attr     = FindAttribute(attributes, Asset::kMeshSemanticTexCoord, 0U);
        auto uv1Attr     = FindAttribute(attributes, Asset::kMeshSemanticTexCoord, 1U);

        auto NormalizeOptionalAttr = [&](FAttributeView& attr) {
            if (!attr.Valid) {
                return;
            }
            const u32 sizeBytes = GetFormatSizeBytes(attr.Desc.Format);
            if (sizeBytes == 0U
                || (attr.Desc.AlignedOffset + sizeBytes) > desc.VertexStride) {
                attr.Valid = false;
            }
        };

        NormalizeOptionalAttr(normalAttr);
        NormalizeOptionalAttr(tangentAttr);
        NormalizeOptionalAttr(uv0Attr);
        NormalizeOptionalAttr(uv1Attr);

        {
            const u32 sizeBytes = GetFormatSizeBytes(positionAttr.Desc.Format);
            if (sizeBytes == 0U
                || (positionAttr.Desc.AlignedOffset + sizeBytes) > desc.VertexStride) {
                return false;
            }
        }

        Core::Container::TVector<Core::Math::FVector3f> positions;
        positions.Reserve(static_cast<usize>(desc.VertexCount));

        const bool hasTangents = tangentAttr.Valid || normalAttr.Valid;
        Core::Container::TVector<Core::Math::FVector4f> tangents;
        if (hasTangents) {
            tangents.Reserve(static_cast<usize>(desc.VertexCount));
        }

        const bool hasUv0 = uv0Attr.Valid;
        Core::Container::TVector<Core::Math::FVector2f> uv0;
        if (hasUv0) {
            uv0.Reserve(static_cast<usize>(desc.VertexCount));
        }

        const bool hasUv1 = uv1Attr.Valid;
        Core::Container::TVector<Core::Math::FVector2f> uv1;
        if (hasUv1) {
            uv1.Reserve(static_cast<usize>(desc.VertexCount));
        }

        const u8* base = vertexData.Data();
        for (u32 i = 0U; i < desc.VertexCount; ++i) {
            const u8* vertex = base + static_cast<usize>(i) * desc.VertexStride;

            {
                f32 values[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
                if (!ReadFloats(vertex + positionAttr.Desc.AlignedOffset, positionAttr.Desc.Format,
                        values, 3U)) {
                    return false;
                }
                positions.PushBack(Core::Math::FVector3f(values[0], values[1], values[2]));
            }

            if (hasTangents) {
                const auto& sourceAttr = tangentAttr.Valid ? tangentAttr : normalAttr;
                f32 values[4]          = { 0.0f, 0.0f, 0.0f, 1.0f };
                if (!ReadFloats(vertex + sourceAttr.Desc.AlignedOffset, sourceAttr.Desc.Format,
                        values, 3U)) {
                    return false;
                }
                tangents.PushBack(Core::Math::FVector4f(values[0], values[1], values[2], 1.0f));
            }

            if (hasUv0) {
                f32 values[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                if (!ReadFloats(vertex + uv0Attr.Desc.AlignedOffset, uv0Attr.Desc.Format, values,
                        2U)) {
                    return false;
                }
                uv0.PushBack(Core::Math::FVector2f(values[0], values[1]));
            }

            if (hasUv1) {
                f32 values[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                if (!ReadFloats(vertex + uv1Attr.Desc.AlignedOffset, uv1Attr.Desc.Format, values,
                        2U)) {
                    return false;
                }
                uv1.PushBack(Core::Math::FVector2f(values[0], values[1]));
            }
        }

        RenderCore::Geometry::FStaticMeshLodData lod{};
        lod.ScreenSize = 1.0f;
        lod.SetPositions(positions.Data(), desc.VertexCount);
        if (!tangents.IsEmpty()) {
            lod.SetTangents(tangents.Data(), desc.VertexCount);
        }
        if (!uv0.IsEmpty()) {
            lod.SetUV0(uv0.Data(), desc.VertexCount);
        }
        if (!uv1.IsEmpty()) {
            lod.SetUV1(uv1.Data(), desc.VertexCount);
        }

        lod.SetIndices(indexData.Data(), desc.IndexCount, indexType);
        lod.PrimitiveTopology = Rhi::ERhiPrimitiveTopology::TriangleList;

        lod.Bounds.Min = Core::Math::FVector3f(
            desc.BoundsMin[0], desc.BoundsMin[1], desc.BoundsMin[2]);
        lod.Bounds.Max = Core::Math::FVector3f(
            desc.BoundsMax[0], desc.BoundsMax[1], desc.BoundsMax[2]);

        if (!subMeshes.IsEmpty()) {
            lod.Sections.Reserve(subMeshes.Size());
            for (const auto& subMesh : subMeshes) {
                RenderCore::Geometry::FStaticMeshSection section{};
                section.FirstIndex   = subMesh.IndexStart;
                section.IndexCount   = subMesh.IndexCount;
                section.BaseVertex   = subMesh.BaseVertex;
                section.MaterialSlot = subMesh.MaterialSlot;
                lod.Sections.PushBack(section);
            }
        } else {
            RenderCore::Geometry::FStaticMeshSection section{};
            section.FirstIndex   = 0U;
            section.IndexCount   = desc.IndexCount;
            section.BaseVertex   = 0;
            section.MaterialSlot = 0U;
            lod.Sections.PushBack(section);
        }

        outMesh.Lods.Clear();
        outMesh.Lods.PushBack(Move(lod));
        outMesh.Bounds = outMesh.Lods[0].Bounds;

        return outMesh.IsValid();
    }
} // namespace AltinaEngine::Rendering
