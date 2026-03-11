#include "Asset/MeshLoader.h"

#include "Asset/AssetBinary.h"
#include "Asset/MeshAsset.h"
#include "Types/NumericProperties.h"
#include "Types/Traits.h"

using AltinaEngine::Forward;
using AltinaEngine::Move;
using AltinaEngine::Core::Container::DestroyPolymorphic;
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

            if (outHeader.mMagic != kAssetBlobMagic || outHeader.mVersion != kAssetBlobVersion) {
                return false;
            }

            if (outHeader.mType != static_cast<u8>(EAssetType::Mesh)) {
                return false;
            }

            if (outHeader.mDescSize != sizeof(FMeshBlobDesc)) {
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
            if (count > (TNumericProperty<u64>::Max / stride)) {
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
            return Container::MakeSharedAs<IAsset, TDerived>(Forward<Args>(args)...);
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

        if (blobDesc.mVertexCount == 0U || blobDesc.mIndexCount == 0U
            || blobDesc.mVertexStride == 0U) {
            return {};
        }

        const u32 indexStride = GetMeshIndexStride(blobDesc.mIndexType);
        if (indexStride == 0U) {
            return {};
        }

        u64 expectedVertexSize = 0;
        if (!TryComputeBytes(blobDesc.mVertexCount, blobDesc.mVertexStride, expectedVertexSize)) {
            return {};
        }
        if (expectedVertexSize != blobDesc.mVertexDataSize) {
            return {};
        }

        u64 expectedIndexSize = 0;
        if (!TryComputeBytes(blobDesc.mIndexCount, indexStride, expectedIndexSize)) {
            return {};
        }
        if (expectedIndexSize != blobDesc.mIndexDataSize) {
            return {};
        }

        const u64 dataSize  = header.mDataSize;
        u64       attrBytes = 0;
        if (!TryComputeBytes(
                blobDesc.mAttributeCount, sizeof(FMeshVertexAttributeDesc), attrBytes)) {
            return {};
        }
        u64 subMeshBytes = 0;
        if (!TryComputeBytes(blobDesc.mSubMeshCount, sizeof(FMeshSubMeshDesc), subMeshBytes)) {
            return {};
        }

        if (!RangeWithin(blobDesc.mAttributesOffset, attrBytes, dataSize)) {
            return {};
        }
        if (!RangeWithin(blobDesc.mSubMeshesOffset, subMeshBytes, dataSize)) {
            return {};
        }
        if (!RangeWithin(blobDesc.mVertexDataOffset, blobDesc.mVertexDataSize, dataSize)) {
            return {};
        }
        if (!RangeWithin(blobDesc.mIndexDataOffset, blobDesc.mIndexDataSize, dataSize)) {
            return {};
        }

        if (desc.mMesh.SubMeshCount != 0U && desc.mMesh.SubMeshCount != blobDesc.mSubMeshCount) {
            return {};
        }
        if (desc.mMesh.IndexFormat != 0U && desc.mMesh.IndexFormat != blobDesc.mIndexType) {
            return {};
        }

        const usize baseOffset = stream.Tell();
        const u64   totalSize  = static_cast<u64>(baseOffset) + dataSize;
        const u64   streamSize = stream.Size();
        if (streamSize != 0U && totalSize > streamSize) {
            return {};
        }

        TVector<FMeshVertexAttributeDesc> attributes;
        if (blobDesc.mAttributeCount > 0U) {
            attributes.Resize(static_cast<usize>(blobDesc.mAttributeCount));
            stream.Seek(baseOffset + static_cast<usize>(blobDesc.mAttributesOffset));
            if (!ReadExact(stream, attributes.Data(), static_cast<usize>(attrBytes))) {
                return {};
            }
        }

        TVector<FMeshSubMeshDesc> subMeshes;
        if (blobDesc.mSubMeshCount > 0U) {
            subMeshes.Resize(static_cast<usize>(blobDesc.mSubMeshCount));
            stream.Seek(baseOffset + static_cast<usize>(blobDesc.mSubMeshesOffset));
            if (!ReadExact(stream, subMeshes.Data(), static_cast<usize>(subMeshBytes))) {
                return {};
            }
        }

        for (const auto& subMesh : subMeshes) {
            const u64 endIndex =
                static_cast<u64>(subMesh.mIndexStart) + static_cast<u64>(subMesh.mIndexCount);
            if (endIndex > blobDesc.mIndexCount) {
                return {};
            }
        }

        TVector<u8> vertexData;
        vertexData.Resize(static_cast<usize>(blobDesc.mVertexDataSize));
        stream.Seek(baseOffset + static_cast<usize>(blobDesc.mVertexDataOffset));
        if (blobDesc.mVertexDataSize > 0U
            && !ReadExact(
                stream, vertexData.Data(), static_cast<usize>(blobDesc.mVertexDataSize))) {
            return {};
        }

        TVector<u8> indexData;
        indexData.Resize(static_cast<usize>(blobDesc.mIndexDataSize));
        stream.Seek(baseOffset + static_cast<usize>(blobDesc.mIndexDataOffset));
        if (blobDesc.mIndexDataSize > 0U
            && !ReadExact(stream, indexData.Data(), static_cast<usize>(blobDesc.mIndexDataSize))) {
            return {};
        }

        FMeshRuntimeDesc runtimeDesc{};
        runtimeDesc.mVertexCount  = blobDesc.mVertexCount;
        runtimeDesc.mIndexCount   = blobDesc.mIndexCount;
        runtimeDesc.mVertexStride = blobDesc.mVertexStride;
        runtimeDesc.mIndexType    = blobDesc.mIndexType;
        runtimeDesc.mFlags        = blobDesc.mFlags;
        runtimeDesc.mBoundsMin[0] = blobDesc.mBoundsMin[0];
        runtimeDesc.mBoundsMin[1] = blobDesc.mBoundsMin[1];
        runtimeDesc.mBoundsMin[2] = blobDesc.mBoundsMin[2];
        runtimeDesc.mBoundsMax[0] = blobDesc.mBoundsMax[0];
        runtimeDesc.mBoundsMax[1] = blobDesc.mBoundsMax[1];
        runtimeDesc.mBoundsMax[2] = blobDesc.mBoundsMax[2];

        return MakeSharedAsset<FMeshAsset>(
            runtimeDesc, Move(attributes), Move(subMeshes), Move(vertexData), Move(indexData));
    }

} // namespace AltinaEngine::Asset
