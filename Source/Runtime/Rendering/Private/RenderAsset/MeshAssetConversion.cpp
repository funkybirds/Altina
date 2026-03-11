#include "RenderAsset/MeshAssetConversion.h"

#include "Asset/AssetBinary.h"
#include "Math/Common.h"
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
                if (attr.mSemantic == semantic && attr.mSemanticIndex == semanticIndex) {
                    return { true, attr };
                }
            }
            return {};
        }

        [[nodiscard]] auto FindAttributeAnyIndex(
            const Core::Container::TVector<FMeshVertexAttributeDesc>& attributes, u32 semantic)
            -> FAttributeView {
            for (const auto& attr : attributes) {
                if (attr.mSemantic == semantic) {
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
                case Asset::kMeshVertexFormatR32Float:
                {
                    formatCount = 1U;
                    break;
                }
                case Asset::kMeshVertexFormatR32G32Float:
                {
                    formatCount = 2U;
                    break;
                }
                case Asset::kMeshVertexFormatR32G32B32Float:
                {
                    formatCount = 3U;
                    break;
                }
                case Asset::kMeshVertexFormatR32G32B32A32Float:
                {
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

        [[nodiscard]] auto NormalizeSafe3(f32 x, f32 y, f32 z) -> Core::Math::FVector3f {
            const f32 lenSq = x * x + y * y + z * z;
            if (lenSq <= 1e-12f) {
                return Core::Math::FVector3f(0.0f, 0.0f, 1.0f);
            }
            const f32 invLen = 1.0f / Core::Math::Sqrt(lenSq);
            return Core::Math::FVector3f(x * invLen, y * invLen, z * invLen);
        }
    } // namespace

    auto ConvertMeshAssetToStaticMesh(
        const Asset::FMeshAsset& asset, RenderCore::Geometry::FStaticMeshData& outMesh) -> bool {
        const auto& desc       = asset.GetDesc();
        const auto& attributes = asset.GetAttributes();
        const auto& subMeshes  = asset.GetSubMeshes();
        const auto& vertexData = asset.GetVertexData();
        const auto& indexData  = asset.GetIndexData();

        if (desc.mVertexCount == 0U || desc.mIndexCount == 0U || desc.mVertexStride == 0U) {
            return false;
        }

        const u64 expectedVertexBytes =
            static_cast<u64>(desc.mVertexCount) * static_cast<u64>(desc.mVertexStride);
        if (vertexData.Size() < static_cast<usize>(expectedVertexBytes)) {
            return false;
        }

        Rhi::ERhiIndexType indexType = Rhi::ERhiIndexType::Uint16;
        if (!ResolveIndexType(desc.mIndexType, indexType)) {
            return false;
        }

        const u32 indexStride =
            RenderCore::Geometry::FStaticMeshLodData::GetIndexStrideBytes(indexType);
        const u64 expectedIndexBytes =
            static_cast<u64>(desc.mIndexCount) * static_cast<u64>(indexStride);
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
            const u32 sizeBytes = GetFormatSizeBytes(attr.Desc.mFormat);
            if (sizeBytes == 0U || (attr.Desc.mAlignedOffset + sizeBytes) > desc.mVertexStride) {
                attr.Valid = false;
            }
        };

        NormalizeOptionalAttr(normalAttr);
        NormalizeOptionalAttr(tangentAttr);
        NormalizeOptionalAttr(uv0Attr);
        NormalizeOptionalAttr(uv1Attr);

        {
            const u32 sizeBytes = GetFormatSizeBytes(positionAttr.Desc.mFormat);
            if (sizeBytes == 0U
                || (positionAttr.Desc.mAlignedOffset + sizeBytes) > desc.mVertexStride) {
                return false;
            }
        }

        Core::Container::TVector<Core::Math::FVector3f> positions;
        positions.Reserve(static_cast<usize>(desc.mVertexCount));

        // NOTE:
        // Base-pass VS consumes slot1 as NORMAL(float3). Keep slot1 stream as tightly packed
        // float3 normals so Vulkan/D3D11 read identical vertex strides.
        const bool                                      hasNormals  = normalAttr.Valid;
        const bool                                      hasTangents = tangentAttr.Valid;
        Core::Container::TVector<Core::Math::FVector3f> packedNormals;
        packedNormals.Reserve(static_cast<usize>(desc.mVertexCount));

        // NOTE:
        // Our base-pass pipeline always expects a TEXCOORD0 stream (input slot 2). If a mesh asset
        // is missing UV0, the renderer will not bind slot 2 which can make the draw invalid under
        // D3D11 validation (and effectively "invisible"). To keep rendering robust, synthesize a
        // zero UV0 buffer when missing.
        const bool                                      hasUv0 = uv0Attr.Valid;
        Core::Container::TVector<Core::Math::FVector2f> uv0;
        uv0.Reserve(static_cast<usize>(desc.mVertexCount));

        const bool                                      hasUv1 = uv1Attr.Valid;
        Core::Container::TVector<Core::Math::FVector2f> uv1;
        if (hasUv1) {
            uv1.Reserve(static_cast<usize>(desc.mVertexCount));
        }

        const u8* base = vertexData.Data();
        for (u32 i = 0U; i < desc.mVertexCount; ++i) {
            const u8* vertex = base + static_cast<usize>(i) * desc.mVertexStride;

            {
                f32 values[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
                if (!ReadFloats(vertex + positionAttr.Desc.mAlignedOffset,
                        positionAttr.Desc.mFormat, values, 3U)) {
                    return false;
                }
                positions.PushBack(Core::Math::FVector3f(values[0], values[1], values[2]));
            }

            {
                f32 values[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
                if (hasNormals) {
                    if (!ReadFloats(vertex + normalAttr.Desc.mAlignedOffset,
                            normalAttr.Desc.mFormat, values, 3U)) {
                        return false;
                    }
                } else if (hasTangents) {
                    if (!ReadFloats(vertex + tangentAttr.Desc.mAlignedOffset,
                            tangentAttr.Desc.mFormat, values, 3U)) {
                        return false;
                    }
                }
                packedNormals.PushBack(NormalizeSafe3(values[0], values[1], values[2]));
            }

            if (hasUv0) {
                f32 values[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                if (!ReadFloats(
                        vertex + uv0Attr.Desc.mAlignedOffset, uv0Attr.Desc.mFormat, values, 2U)) {
                    return false;
                }
                uv0.PushBack(Core::Math::FVector2f(values[0], values[1]));
            } else {
                uv0.PushBack(Core::Math::FVector2f(0.0f, 0.0f));
            }

            if (hasUv1) {
                f32 values[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                if (!ReadFloats(
                        vertex + uv1Attr.Desc.mAlignedOffset, uv1Attr.Desc.mFormat, values, 2U)) {
                    return false;
                }
                uv1.PushBack(Core::Math::FVector2f(values[0], values[1]));
            }
        }

        RenderCore::Geometry::FStaticMeshLodData lod{};
        lod.mScreenSize = 1.0f;
        lod.SetPositions(positions.Data(), desc.mVertexCount);
        lod.mTangentBuffer.SetData(packedNormals.Data(),
            desc.mVertexCount * static_cast<u32>(sizeof(Core::Math::FVector3f)),
            static_cast<u32>(sizeof(Core::Math::FVector3f)));
        // Always provide UV0 (see note above).
        lod.SetUV0(uv0.Data(), desc.mVertexCount);
        if (!uv1.IsEmpty()) {
            lod.SetUV1(uv1.Data(), desc.mVertexCount);
        }

        lod.SetIndices(indexData.Data(), desc.mIndexCount, indexType);
        lod.mPrimitiveTopology = Rhi::ERhiPrimitiveTopology::TriangleList;

        lod.mBounds.Min =
            Core::Math::FVector3f(desc.mBoundsMin[0], desc.mBoundsMin[1], desc.mBoundsMin[2]);
        lod.mBounds.Max =
            Core::Math::FVector3f(desc.mBoundsMax[0], desc.mBoundsMax[1], desc.mBoundsMax[2]);

        if (!subMeshes.IsEmpty()) {
            lod.mSections.Reserve(subMeshes.Size());
            for (const auto& subMesh : subMeshes) {
                RenderCore::Geometry::FStaticMeshSection section{};
                section.FirstIndex   = subMesh.mIndexStart;
                section.IndexCount   = subMesh.mIndexCount;
                section.BaseVertex   = subMesh.mBaseVertex;
                section.MaterialSlot = subMesh.mMaterialSlot;
                lod.mSections.PushBack(section);
            }
        } else {
            RenderCore::Geometry::FStaticMeshSection section{};
            section.FirstIndex   = 0U;
            section.IndexCount   = desc.mIndexCount;
            section.BaseVertex   = 0;
            section.MaterialSlot = 0U;
            lod.mSections.PushBack(section);
        }

        outMesh.mLods.Clear();
        outMesh.mLods.PushBack(Move(lod));
        outMesh.mBounds = outMesh.mLods[0].mBounds;

        return outMesh.IsValid();
    }
} // namespace AltinaEngine::Rendering
