
#include "Importers/Model/ModelImporter.h"

#include "Asset/AssetBinary.h"
#include "Importers/Model/FbxImporter.h"
#include "Importers/Model/PmxImporter.h"
#include "Importers/Model/UsdzImporter.h"

#include <cstring>
#include <limits>

using AltinaEngine::Move;
namespace AltinaEngine::Tools::AssetPipeline {
    namespace {
        auto ToLowerAscii(char value) -> char {
            if (value >= 'A' && value <= 'Z') {
                return static_cast<char>(value - 'A' + 'a');
            }
            return value;
        }

        void ToLowerAscii(std::string& value) {
            for (char& ch : value) {
                ch = ToLowerAscii(ch);
            }
        }

        auto NormalizeExt(const std::filesystem::path& path) -> std::string {
            std::string ext = path.extension().string();
            ToLowerAscii(ext);
            return ext;
        }

        auto TryLoadCookedModelFromBytes(
            const std::vector<u8>& sourceBytes, FModelCookResult& outResult) -> bool {
            if (sourceBytes.size()
                < sizeof(Asset::FAssetBlobHeader) + sizeof(Asset::FModelBlobDesc)) {
                return false;
            }

            Asset::FAssetBlobHeader header{};
            std::memcpy(&header, sourceBytes.data(), sizeof(header));

            if (header.mMagic != Asset::kAssetBlobMagic
                || header.mVersion != Asset::kAssetBlobVersion
                || header.mType != static_cast<u8>(Asset::EAssetType::Model)
                || header.mDescSize != sizeof(Asset::FModelBlobDesc)) {
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
            if (!TryComputeBytes(blobDesc.mNodeCount, sizeof(Asset::FModelNodeDesc), nodesBytes)) {
                return false;
            }
            u64 meshRefBytes = 0;
            if (!TryComputeBytes(
                    blobDesc.mMeshRefCount, sizeof(Asset::FModelMeshRef), meshRefBytes)) {
                return false;
            }
            u64 materialBytes = 0;
            if (!TryComputeBytes(
                    blobDesc.mMaterialSlotCount, sizeof(Asset::FAssetHandle), materialBytes)) {
                return false;
            }

            const u64 dataSize = header.mDataSize;
            if (!RangeWithin(blobDesc.mNodesOffset, nodesBytes, dataSize)
                || !RangeWithin(blobDesc.mMeshRefsOffset, meshRefBytes, dataSize)
                || !RangeWithin(blobDesc.mMaterialSlotsOffset, materialBytes, dataSize)) {
                return false;
            }

            const u64 totalSize =
                sizeof(Asset::FAssetBlobHeader) + sizeof(Asset::FModelBlobDesc) + dataSize;
            if (sourceBytes.size() < totalSize) {
                return false;
            }

            outResult.Desc.NodeCount         = blobDesc.mNodeCount;
            outResult.Desc.MeshRefCount      = blobDesc.mMeshRefCount;
            outResult.Desc.MaterialSlotCount = blobDesc.mMaterialSlotCount;
            outResult.CookedBytes            = sourceBytes;
            outResult.CookKeyExtras          = sourceBytes;
            return true;
        }
    } // namespace

    auto CookModel(const std::filesystem::path& sourcePath, const std::vector<u8>& sourceBytes,
        const Asset::FAssetHandle& baseHandle, const std::string& baseVirtualPath,
        FModelCookResult& outResult, std::string& outError) -> bool {
        outResult = {};
        outError.clear();

        if (TryLoadCookedModelFromBytes(sourceBytes, outResult)) {
            return true;
        }

        const std::string ext = NormalizeExt(sourcePath);
        if (ext == ".fbx") {
            return CookModelFromFbx(sourcePath, baseHandle, baseVirtualPath, outResult, outError);
        }
        if (ext == ".pmx") {
            return CookModelFromPmx(sourcePath, baseHandle, baseVirtualPath, outResult, outError);
        }
        if (ext == ".usdz") {
            return CookModelFromUsdz(sourcePath, baseHandle, baseVirtualPath, outResult, outError);
        }

        outError = "Unsupported model format.";
        return false;
    }
} // namespace AltinaEngine::Tools::AssetPipeline
