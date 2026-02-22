#include "Asset/ModelLoader.h"

#include "Asset/AssetBinary.h"
#include "Asset/ModelAsset.h"
#include "Types/Traits.h"

#include <limits>

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

            if (outHeader.Magic != kAssetBlobMagic || outHeader.Version != kAssetBlobVersion) {
                return false;
            }

            if (outHeader.Type != static_cast<u8>(EAssetType::Model)) {
                return false;
            }

            if (outHeader.DescSize != sizeof(FModelBlobDesc)) {
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
                        allocator, ptr, Forward<Args>(args)...);
                } catch (...) {
                    TAllocatorTraits<TAllocator<TDerived>>::Deallocate(allocator, ptr, 1);
                    return {};
                }
            } else {
                ptr = new TDerived(Forward<Args>(args)...); // NOLINT
            }

            return TShared<IAsset>(
                ptr, TPolymorphicDeleter<IAsset>(&DestroyPolymorphic<IAsset, TDerived>));
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
        if (!TryComputeBytes(blobDesc.NodeCount, sizeof(FModelNodeDesc), nodesBytes)) {
            return {};
        }
        u64 meshRefBytes = 0;
        if (!TryComputeBytes(blobDesc.MeshRefCount, sizeof(FModelMeshRef), meshRefBytes)) {
            return {};
        }
        u64 materialBytes = 0;
        if (!TryComputeBytes(blobDesc.MaterialSlotCount, sizeof(FAssetHandle), materialBytes)) {
            return {};
        }

        const u64 dataSize = header.DataSize;
        if (!RangeWithin(blobDesc.NodesOffset, nodesBytes, dataSize)) {
            return {};
        }
        if (!RangeWithin(blobDesc.MeshRefsOffset, meshRefBytes, dataSize)) {
            return {};
        }
        if (!RangeWithin(blobDesc.MaterialSlotsOffset, materialBytes, dataSize)) {
            return {};
        }

        if (desc.Model.NodeCount != 0U && desc.Model.NodeCount != blobDesc.NodeCount) {
            return {};
        }
        if (desc.Model.MeshRefCount != 0U && desc.Model.MeshRefCount != blobDesc.MeshRefCount) {
            return {};
        }
        if (desc.Model.MaterialSlotCount != 0U
            && desc.Model.MaterialSlotCount != blobDesc.MaterialSlotCount) {
            return {};
        }

        const usize baseOffset = stream.Tell();
        const u64   totalSize  = static_cast<u64>(baseOffset) + dataSize;
        const u64   streamSize = stream.Size();
        if (streamSize != 0U && totalSize > streamSize) {
            return {};
        }

        TVector<FModelNodeDesc> nodes;
        if (blobDesc.NodeCount > 0U) {
            nodes.Resize(static_cast<usize>(blobDesc.NodeCount));
            stream.Seek(baseOffset + static_cast<usize>(blobDesc.NodesOffset));
            if (!ReadExact(stream, nodes.Data(), static_cast<usize>(nodesBytes))) {
                return {};
            }
        }

        TVector<FModelMeshRef> meshRefs;
        if (blobDesc.MeshRefCount > 0U) {
            meshRefs.Resize(static_cast<usize>(blobDesc.MeshRefCount));
            stream.Seek(baseOffset + static_cast<usize>(blobDesc.MeshRefsOffset));
            if (!ReadExact(stream, meshRefs.Data(), static_cast<usize>(meshRefBytes))) {
                return {};
            }
        }

        TVector<FAssetHandle> materialSlots;
        if (blobDesc.MaterialSlotCount > 0U) {
            materialSlots.Resize(static_cast<usize>(blobDesc.MaterialSlotCount));
            stream.Seek(baseOffset + static_cast<usize>(blobDesc.MaterialSlotsOffset));
            if (!ReadExact(stream, materialSlots.Data(), static_cast<usize>(materialBytes))) {
                return {};
            }
        }

        FModelRuntimeDesc runtimeDesc{};
        runtimeDesc.NodeCount         = blobDesc.NodeCount;
        runtimeDesc.MeshRefCount      = blobDesc.MeshRefCount;
        runtimeDesc.MaterialSlotCount = blobDesc.MaterialSlotCount;

        return MakeSharedAsset<FModelAsset>(
            runtimeDesc, Move(nodes), Move(meshRefs), Move(materialSlots));
    }

} // namespace AltinaEngine::Asset
