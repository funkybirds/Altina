#include "Asset/LevelLoader.h"

#include "Asset/AssetBinary.h"
#include "Asset/LevelAsset.h"

using AltinaEngine::Move;
namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    namespace {
        auto ReadExact(IAssetStream& stream, void* outBuffer, usize size) -> bool {
            if (outBuffer == nullptr || size == 0U) {
                return false;
            }

            auto*       out       = static_cast<u8*>(outBuffer);
            usize       totalRead = 0U;
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
    } // namespace

    auto FLevelLoader::CanLoad(EAssetType type) const noexcept -> bool {
        return type == EAssetType::Level;
    }

    auto FLevelLoader::Load(const FAssetDesc& desc, IAssetStream& stream) -> TShared<IAsset> {
        FAssetBlobHeader header{};
        if (!ReadExact(stream, &header, sizeof(FAssetBlobHeader))) {
            return {};
        }

        if (header.mMagic != kAssetBlobMagic || header.mVersion != kAssetBlobVersion) {
            return {};
        }
        if (header.mType != static_cast<u8>(EAssetType::Level)) {
            return {};
        }
        if (header.mDescSize != sizeof(FLevelBlobDesc)) {
            return {};
        }

        FLevelBlobDesc blobDesc{};
        if (!ReadExact(stream, &blobDesc, sizeof(FLevelBlobDesc))) {
            return {};
        }

        TVector<u8> payload{};
        if (header.mDataSize > 0U) {
            payload.Resize(static_cast<usize>(header.mDataSize));
            if (!ReadExact(stream, payload.Data(), static_cast<usize>(header.mDataSize))) {
                return {};
            }
        }

        if (desc.mLevel.Encoding != 0U && desc.mLevel.Encoding != blobDesc.mEncoding) {
            return {};
        }
        if (desc.mLevel.ByteSize != 0U
            && desc.mLevel.ByteSize != static_cast<u32>(payload.Size())) {
            return {};
        }

        return Container::MakeSharedAs<IAsset, FLevelAsset>(blobDesc.mEncoding, Move(payload));
    }
} // namespace AltinaEngine::Asset
