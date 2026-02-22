#include "Importers/Model/ModelImporter.h"

#include "Asset/AssetBinary.h"

#include <cstring>
#include <limits>

namespace AltinaEngine::Tools::AssetPipeline {
    auto CookModel(const std::vector<u8>& sourceBytes, std::vector<u8>& outCooked,
        Asset::FModelDesc& outDesc) -> bool {
        if (sourceBytes.size() < sizeof(Asset::FAssetBlobHeader) + sizeof(Asset::FModelBlobDesc)) {
            return false;
        }

        Asset::FAssetBlobHeader header{};
        std::memcpy(&header, sourceBytes.data(), sizeof(header));
        if (header.Magic != Asset::kAssetBlobMagic || header.Version != Asset::kAssetBlobVersion) {
            return false;
        }
        if (header.Type != static_cast<u8>(Asset::EAssetType::Model)) {
            return false;
        }
        if (header.DescSize != sizeof(Asset::FModelBlobDesc)) {
            return false;
        }

        Asset::FModelBlobDesc blobDesc{};
        std::memcpy(&blobDesc, sourceBytes.data() + sizeof(header), sizeof(blobDesc));

        auto TryComputeBytes = [](u64 count, u64 stride, u64& outBytes) -> bool {
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
        };

        auto RangeWithin = [](u64 offset, u64 size, u64 dataSize) -> bool {
            return offset <= dataSize && size <= (dataSize - offset);
        };

        u64 nodesBytes = 0;
        if (!TryComputeBytes(blobDesc.NodeCount, sizeof(Asset::FModelNodeDesc), nodesBytes)) {
            return false;
        }
        u64 meshRefBytes = 0;
        if (!TryComputeBytes(blobDesc.MeshRefCount, sizeof(Asset::FModelMeshRef), meshRefBytes)) {
            return false;
        }
        u64 materialBytes = 0;
        if (!TryComputeBytes(
                blobDesc.MaterialSlotCount, sizeof(Asset::FAssetHandle), materialBytes)) {
            return false;
        }

        const u64 dataSize = header.DataSize;
        if (!RangeWithin(blobDesc.NodesOffset, nodesBytes, dataSize)
            || !RangeWithin(blobDesc.MeshRefsOffset, meshRefBytes, dataSize)
            || !RangeWithin(blobDesc.MaterialSlotsOffset, materialBytes, dataSize)) {
            return false;
        }

        const u64 totalSize =
            sizeof(Asset::FAssetBlobHeader) + sizeof(Asset::FModelBlobDesc) + dataSize;
        if (sourceBytes.size() < totalSize) {
            return false;
        }

        outDesc.NodeCount         = blobDesc.NodeCount;
        outDesc.MeshRefCount      = blobDesc.MeshRefCount;
        outDesc.MaterialSlotCount = blobDesc.MaterialSlotCount;
        outCooked                 = sourceBytes;
        return true;
    }
} // namespace AltinaEngine::Tools::AssetPipeline
