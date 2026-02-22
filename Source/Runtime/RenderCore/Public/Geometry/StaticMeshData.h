#pragma once

#include <limits>

#include "RenderCoreAPI.h"

#include "Container/Vector.h"
#include "Math/Vector.h"
#include "RenderResource.h"
#include "Types/Aliases.h"

namespace AltinaEngine::RenderCore::Geometry {
    namespace Container = Core::Container;
    namespace Math      = Core::Math;
    using Container::TVector;

    struct AE_RENDER_CORE_API FStaticMeshBounds3f {
        // Core::Math::TVector declares custom constructors, so it has no implicit default ctor.
        // Initialize to an invalid bounds by default so IsValid() works as expected.
        Math::FVector3f              Min{ std::numeric_limits<f32>::max() };
        Math::FVector3f              Max{ -std::numeric_limits<f32>::max() };

        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool {
            return (Min[0] <= Max[0]) && (Min[1] <= Max[1]) && (Min[2] <= Max[2]);
        }
    };

    struct AE_RENDER_CORE_API FStaticMeshSection {
        u32                          FirstIndex   = 0U;
        u32                          IndexCount   = 0U;
        i32                          BaseVertex   = 0;
        u32                          MaterialSlot = 0U;

        [[nodiscard]] constexpr auto GetTriangleCount(
            Rhi::ERhiPrimitiveTopology topology) const noexcept -> u32 {
            switch (topology) {
                case Rhi::ERhiPrimitiveTopology::TriangleList:
                    return IndexCount / 3U;
                case Rhi::ERhiPrimitiveTopology::TriangleStrip:
                    return IndexCount >= 3U ? (IndexCount - 2U) : 0U;
                default:
                    return 0U;
            }
        }
    };

    struct AE_RENDER_CORE_API FStaticMeshLodData {
        FStaticMeshLodData()                                                            = default;
        FStaticMeshLodData(const FStaticMeshLodData&)                                   = delete;
        auto operator=(const FStaticMeshLodData&) -> FStaticMeshLodData&                = delete;
        FStaticMeshLodData(FStaticMeshLodData&&) noexcept                               = default;
        auto            operator=(FStaticMeshLodData&&) noexcept -> FStaticMeshLodData& = default;

        f32             ScreenSize = 1.0f;

        FPositionBuffer PositionBuffer;
        FVertexTangentBuffer        TangentBuffer;
        FVertexUVBuffer             UV0Buffer;
        FVertexUVBuffer             UV1Buffer;
        FIndexBuffer                IndexBuffer;

        Rhi::ERhiPrimitiveTopology  PrimitiveTopology = Rhi::ERhiPrimitiveTopology::TriangleList;

        TVector<FStaticMeshSection> Sections;
        FStaticMeshBounds3f         Bounds;

        void                        SetPositions(const Math::FVector3f* data, u32 count) {
            PositionBuffer.SetData(
                data, count * static_cast<u32>(sizeof(Math::FVector3f)), sizeof(Math::FVector3f));
        }

        void SetTangents(const Math::FVector4f* data, u32 count) {
            TangentBuffer.SetData(
                data, count * static_cast<u32>(sizeof(Math::FVector4f)), sizeof(Math::FVector4f));
        }

        void SetUV0(const Math::FVector2f* data, u32 count) {
            UV0Buffer.SetData(
                data, count * static_cast<u32>(sizeof(Math::FVector2f)), sizeof(Math::FVector2f));
        }

        void SetUV1(const Math::FVector2f* data, u32 count) {
            UV1Buffer.SetData(
                data, count * static_cast<u32>(sizeof(Math::FVector2f)), sizeof(Math::FVector2f));
        }

        void SetIndices(const void* data, u32 count, Rhi::ERhiIndexType indexType) {
            const u32 strideBytes = GetIndexStrideBytes(indexType);
            IndexBuffer.SetData(data, count * strideBytes, indexType);
        }

        [[nodiscard]] auto GetVertexCount() const noexcept -> u32 {
            const u32 stride = PositionBuffer.GetStrideBytes();
            const u64 size   = PositionBuffer.GetSizeBytes();
            if (stride == 0U || (size % static_cast<u64>(stride)) != 0ULL) {
                return 0U;
            }
            return static_cast<u32>(size / static_cast<u64>(stride));
        }

        [[nodiscard]] auto GetIndexCount() const noexcept -> u32 {
            const u32 stride = GetIndexStrideBytes(IndexBuffer.GetIndexType());
            const u64 size   = IndexBuffer.GetSizeBytes();
            if (stride == 0U || (size % static_cast<u64>(stride)) != 0ULL) {
                return 0U;
            }
            return static_cast<u32>(size / static_cast<u64>(stride));
        }

        [[nodiscard]] constexpr auto GetPositionStrideBytes() const noexcept -> u32 {
            return static_cast<u32>(sizeof(Math::FVector3f));
        }

        [[nodiscard]] constexpr auto GetTangentStrideBytes() const noexcept -> u32 {
            return static_cast<u32>(sizeof(Math::FVector4f));
        }

        [[nodiscard]] constexpr auto GetUVStrideBytes() const noexcept -> u32 {
            return static_cast<u32>(sizeof(Math::FVector2f));
        }

        [[nodiscard]] static constexpr auto GetIndexStrideBytes(
            Rhi::ERhiIndexType indexType) noexcept -> u32 {
            switch (indexType) {
                case Rhi::ERhiIndexType::Uint16:
                    return 2U;
                case Rhi::ERhiIndexType::Uint32:
                    return 4U;
                default:
                    return 0U;
            }
        }

        [[nodiscard]] auto IsValid() const noexcept -> bool {
            const u32 vertexCount = GetVertexCount();
            if (vertexCount == 0U) {
                return false;
            }

            if (TangentBuffer.GetSizeBytes() != 0ULL
                && TangentBuffer.GetElementCount() != vertexCount) {
                return false;
            }
            if (UV0Buffer.GetSizeBytes() != 0ULL && UV0Buffer.GetElementCount() != vertexCount) {
                return false;
            }
            if (UV1Buffer.GetSizeBytes() != 0ULL && UV1Buffer.GetElementCount() != vertexCount) {
                return false;
            }

            const u32 indexCount = GetIndexCount();
            if (indexCount == 0U) {
                return false;
            }

            const u32 stride = GetIndexStrideBytes(IndexBuffer.GetIndexType());
            if (stride == 0U) {
                return false;
            }

            for (const auto& section : Sections) {
                const u64 endIndex =
                    static_cast<u64>(section.FirstIndex) + static_cast<u64>(section.IndexCount);
                if (endIndex > indexCount) {
                    return false;
                }
            }

            return true;
        }
    };

    struct AE_RENDER_CORE_API FStaticMeshData {
        TVector<FStaticMeshLodData> Lods;
        FStaticMeshBounds3f         Bounds;

        [[nodiscard]] auto          GetLodCount() const noexcept -> u32 {
            return static_cast<u32>(Lods.Size());
        }

        [[nodiscard]] auto IsValid() const noexcept -> bool {
            if (Lods.IsEmpty()) {
                return false;
            }

            for (const auto& lod : Lods) {
                if (!lod.IsValid()) {
                    return false;
                }
            }

            return true;
        }
    };

} // namespace AltinaEngine::RenderCore::Geometry
