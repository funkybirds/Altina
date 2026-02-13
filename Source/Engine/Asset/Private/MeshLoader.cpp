#include "Asset/MeshLoader.h"

#include "Asset/AssetBinary.h"
#include "Asset/MeshAsset.h"
#include "Types/Traits.h"

#include <limits>

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    namespace {
        auto ReadExact(IAssetStream& stream, void* outBuffer, usize size) -> bool {
            if (outBuffer == nullptr || size == 0U) {
                return false;
            }

            auto*       out       = static_cast<u8*>(outBuffer);
            usize       totalRead = 0;
            const usize target    = size;
            while (totalRead < target) {
                const usize read = stream.Read(out + totalRead, target - totalRead);
                if (read == 0U) {
                    return false;
                }
                totalRead += read;
            }
            return true;
        }

        auto ReadHeader(IAssetStream& stream, FAssetBlobHeader& outHeader) -> bool {
            if (!ReadExact(stream, &outHeader, sizeof(FAssetBlobHeader))) {
                return false;
            }

            if (outHeader.Magic != kAssetBlobMagic || outHeader.Version != kAssetBlobVersion) {
                return false;
            }

            if (outHeader.Type != static_cast<u8>(EAssetType::Mesh)) {
                return false;
            }

            if (outHeader.DescSize != sizeof(FMeshBlobDesc)) {
                return false;
            }

            return true;
        }

        auto TryComputeBytes(u64 count, u64 stride, u64& outBytes) noexcept -> bool {
            if (count == 0U) {
                outBytes = 0U;
                return true;
            }
            if (stride == 0U) {
                return false;
            }
            if (count > (std::numeric_limits<u64>::max() / stride)) {
                return false;
            }
            outBytes = count * stride;
            return true;
        }

        auto RangeWithin(u64 offset, u64 size, u64 dataSize) noexcept -> bool {
            return offset <= dataSize && size <= (dataSize - offset);
        }

        template <typename TDerived, typename... Args>
        auto MakeSharedAsset(Args&&... args) -> TShared<IAsset> {
            using Container::kSmartPtrUseManagedAllocator;
            using Container::TAllocator;
            using Container::TAllocatorTraits;
            using Container::TPolymorphicDeleter;

            TDerived* ptr = nullptr;
            if constexpr (kSmartPtrUseManagedAllocator) {
                TAllocator<TDerived> allocator;
                ptr = TAllocatorTraits<TAllocator<TDerived>>::Allocate(allocator, 1);
                if (ptr == nullptr) {
                    return {};
                }

                try {
                    TAllocatorTraits<TAllocator<TDerived>>::Construct(
                        allocator, ptr, AltinaEngine::Forward<Args>(args)...);
                } catch (...) {
                    TAllocatorTraits<TAllocator<TDerived>>::Deallocate(allocator, ptr, 1);
                    return {};
                }
            } else {
                ptr = new TDerived(AltinaEngine::Forward<Args>(args)...); // NOLINT
            }

            return TShared<IAsset>(
                ptr, TPolymorphicDeleter<IAsset>(&Container::DestroyPolymorphic<IAsset, TDerived>));
        }
    } // namespace

    auto FMeshLoader::CanLoad(EAssetType type) const noexcept -> bool {
        return type == EAssetType::Mesh;
    }

    auto FMeshLoader::Load(const FAssetDesc& desc, IAssetStream& stream) -> TShared<IAsset> {
        FAssetBlobHeader header{};
        if (!ReadHeader(stream, header)) {
            return {};
        }

        FMeshBlobDesc blobDesc{};
        if (!ReadExact(stream, &blobDesc, sizeof(FMeshBlobDesc))) {
            return {};
        }

        if (blobDesc.VertexCount == 0U || blobDesc.IndexCount == 0U
            || blobDesc.VertexStride == 0U) {
            return {};
        }

        const u32 indexStride = GetMeshIndexStride(blobDesc.IndexType);
        if (indexStride == 0U) {
            return {};
        }

        u64 expectedVertexSize = 0;
        if (!TryComputeBytes(blobDesc.VertexCount, blobDesc.VertexStride, expectedVertexSize)) {
            return {};
        }
        if (expectedVertexSize != blobDesc.VertexDataSize) {
            return {};
        }

        u64 expectedIndexSize = 0;
        if (!TryComputeBytes(blobDesc.IndexCount, indexStride, expectedIndexSize)) {
            return {};
        }
        if (expectedIndexSize != blobDesc.IndexDataSize) {
            return {};
        }

        const u64 dataSize  = header.DataSize;
        u64       attrBytes = 0;
        if (!TryComputeBytes(
                blobDesc.AttributeCount, sizeof(FMeshVertexAttributeDesc), attrBytes)) {
            return {};
        }
        u64 subMeshBytes = 0;
        if (!TryComputeBytes(blobDesc.SubMeshCount, sizeof(FMeshSubMeshDesc), subMeshBytes)) {
            return {};
        }

        if (!RangeWithin(blobDesc.AttributesOffset, attrBytes, dataSize)) {
            return {};
        }
        if (!RangeWithin(blobDesc.SubMeshesOffset, subMeshBytes, dataSize)) {
            return {};
        }
        if (!RangeWithin(blobDesc.VertexDataOffset, blobDesc.VertexDataSize, dataSize)) {
            return {};
        }
        if (!RangeWithin(blobDesc.IndexDataOffset, blobDesc.IndexDataSize, dataSize)) {
            return {};
        }

        if (desc.Mesh.SubMeshCount != 0U && desc.Mesh.SubMeshCount != blobDesc.SubMeshCount) {
            return {};
        }
        if (desc.Mesh.IndexFormat != 0U && desc.Mesh.IndexFormat != blobDesc.IndexType) {
            return {};
        }

        const usize baseOffset = stream.Tell();
        const u64   totalSize  = static_cast<u64>(baseOffset) + dataSize;
        const u64   streamSize = stream.Size();
        if (streamSize != 0U && totalSize > streamSize) {
            return {};
        }

        TVector<FMeshVertexAttributeDesc> attributes;
        if (blobDesc.AttributeCount > 0U) {
            attributes.Resize(static_cast<usize>(blobDesc.AttributeCount));
            stream.Seek(baseOffset + static_cast<usize>(blobDesc.AttributesOffset));
            if (!ReadExact(stream, attributes.Data(), static_cast<usize>(attrBytes))) {
                return {};
            }
        }

        TVector<FMeshSubMeshDesc> subMeshes;
        if (blobDesc.SubMeshCount > 0U) {
            subMeshes.Resize(static_cast<usize>(blobDesc.SubMeshCount));
            stream.Seek(baseOffset + static_cast<usize>(blobDesc.SubMeshesOffset));
            if (!ReadExact(stream, subMeshes.Data(), static_cast<usize>(subMeshBytes))) {
                return {};
            }
        }

        for (const auto& subMesh : subMeshes) {
            const u64 endIndex =
                static_cast<u64>(subMesh.IndexStart) + static_cast<u64>(subMesh.IndexCount);
            if (endIndex > blobDesc.IndexCount) {
                return {};
            }
        }

        TVector<u8> vertexData;
        vertexData.Resize(static_cast<usize>(blobDesc.VertexDataSize));
        stream.Seek(baseOffset + static_cast<usize>(blobDesc.VertexDataOffset));
        if (blobDesc.VertexDataSize > 0U
            && !ReadExact(stream, vertexData.Data(), static_cast<usize>(blobDesc.VertexDataSize))) {
            return {};
        }

        TVector<u8> indexData;
        indexData.Resize(static_cast<usize>(blobDesc.IndexDataSize));
        stream.Seek(baseOffset + static_cast<usize>(blobDesc.IndexDataOffset));
        if (blobDesc.IndexDataSize > 0U
            && !ReadExact(stream, indexData.Data(), static_cast<usize>(blobDesc.IndexDataSize))) {
            return {};
        }

        FMeshRuntimeDesc runtimeDesc{};
        runtimeDesc.VertexCount  = blobDesc.VertexCount;
        runtimeDesc.IndexCount   = blobDesc.IndexCount;
        runtimeDesc.VertexStride = blobDesc.VertexStride;
        runtimeDesc.IndexType    = blobDesc.IndexType;
        runtimeDesc.Flags        = blobDesc.Flags;
        runtimeDesc.BoundsMin[0] = blobDesc.BoundsMin[0];
        runtimeDesc.BoundsMin[1] = blobDesc.BoundsMin[1];
        runtimeDesc.BoundsMin[2] = blobDesc.BoundsMin[2];
        runtimeDesc.BoundsMax[0] = blobDesc.BoundsMax[0];
        runtimeDesc.BoundsMax[1] = blobDesc.BoundsMax[1];
        runtimeDesc.BoundsMax[2] = blobDesc.BoundsMax[2];

        return MakeSharedAsset<FMeshAsset>(runtimeDesc, AltinaEngine::Move(attributes),
            AltinaEngine::Move(subMeshes), AltinaEngine::Move(vertexData),
            AltinaEngine::Move(indexData));
    }

} // namespace AltinaEngine::Asset
