#include "Asset/ModelLoader.h"

#include "Asset/AssetBinary.h"
#include "Asset/ModelAsset.h"
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

            if (outHeader.mType != static_cast<u8>(EAssetType::Model)) {
                return false;
            }

            if (outHeader.mDescSize != sizeof(FModelBlobDesc)) {
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

    auto FModelLoader::CanLoad(EAssetType type) const noexcept -> bool {
        return type == EAssetType::Model;
    }

    auto FModelLoader::Load(const FAssetDesc& desc, IAssetStream& stream) -> TShared<IAsset> {
        FAssetBlobHeader header{};
        if (!ReadHeader(stream, header)) {
            return {};
        }

        FModelBlobDesc blobDesc{};
        if (!ReadExact(stream, &blobDesc, sizeof(FModelBlobDesc))) {
            return {};
        }

        u64 nodesBytes = 0;
        if (!TryComputeBytes(blobDesc.mNodeCount, sizeof(FModelNodeDesc), nodesBytes)) {
            return {};
        }
        u64 meshRefBytes = 0;
        if (!TryComputeBytes(blobDesc.mMeshRefCount, sizeof(FModelMeshRef), meshRefBytes)) {
            return {};
        }
        u64 materialBytes = 0;
        if (!TryComputeBytes(blobDesc.mMaterialSlotCount, sizeof(FAssetHandle), materialBytes)) {
            return {};
        }

        const u64 dataSize = header.mDataSize;
        if (!RangeWithin(blobDesc.mNodesOffset, nodesBytes, dataSize)) {
            return {};
        }
        if (!RangeWithin(blobDesc.mMeshRefsOffset, meshRefBytes, dataSize)) {
            return {};
        }
        if (!RangeWithin(blobDesc.mMaterialSlotsOffset, materialBytes, dataSize)) {
            return {};
        }

        if (desc.mModel.NodeCount != 0U && desc.mModel.NodeCount != blobDesc.mNodeCount) {
            return {};
        }
        if (desc.mModel.MeshRefCount != 0U && desc.mModel.MeshRefCount != blobDesc.mMeshRefCount) {
            return {};
        }
        if (desc.mModel.MaterialSlotCount != 0U
            && desc.mModel.MaterialSlotCount != blobDesc.mMaterialSlotCount) {
            return {};
        }

        const usize baseOffset = stream.Tell();
        const u64   totalSize  = static_cast<u64>(baseOffset) + dataSize;
        const u64   streamSize = stream.Size();
        if (streamSize != 0U && totalSize > streamSize) {
            return {};
        }

        TVector<FModelNodeDesc> nodes;
        if (blobDesc.mNodeCount > 0U) {
            nodes.Resize(static_cast<usize>(blobDesc.mNodeCount));
            stream.Seek(baseOffset + static_cast<usize>(blobDesc.mNodesOffset));
            if (!ReadExact(stream, nodes.Data(), static_cast<usize>(nodesBytes))) {
                return {};
            }
        }

        TVector<FModelMeshRef> meshRefs;
        if (blobDesc.mMeshRefCount > 0U) {
            meshRefs.Resize(static_cast<usize>(blobDesc.mMeshRefCount));
            stream.Seek(baseOffset + static_cast<usize>(blobDesc.mMeshRefsOffset));
            if (!ReadExact(stream, meshRefs.Data(), static_cast<usize>(meshRefBytes))) {
                return {};
            }
        }

        TVector<FAssetHandle> materialSlots;
        if (blobDesc.mMaterialSlotCount > 0U) {
            materialSlots.Resize(static_cast<usize>(blobDesc.mMaterialSlotCount));
            stream.Seek(baseOffset + static_cast<usize>(blobDesc.mMaterialSlotsOffset));
            if (!ReadExact(stream, materialSlots.Data(), static_cast<usize>(materialBytes))) {
                return {};
            }
        }

        FModelRuntimeDesc runtimeDesc{};
        runtimeDesc.mNodeCount         = blobDesc.mNodeCount;
        runtimeDesc.mMeshRefCount      = blobDesc.mMeshRefCount;
        runtimeDesc.mMaterialSlotCount = blobDesc.mMaterialSlotCount;

        return MakeSharedAsset<FModelAsset>(
            runtimeDesc, Move(nodes), Move(meshRefs), Move(materialSlots));
    }

} // namespace AltinaEngine::Asset
