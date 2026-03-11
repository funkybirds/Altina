#include "Asset/CubeMapLoader.h"

#include "Asset/AssetBinary.h"
#include "Asset/CubeMapAsset.h"
#include "Types/NumericProperties.h"

using AltinaEngine::Forward;
using AltinaEngine::Move;
using AltinaEngine::Core::Container::DestroyPolymorphic;
namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    namespace {
        constexpr u32 kCubeMapFaceCount = 6U;

        auto          ReadExact(IAssetStream& stream, void* outBuffer, usize size) -> bool {
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

        auto HasCompleteCubeMapDesc(const FCubeMapDesc& desc) noexcept -> bool {
            return desc.Size > 0U && desc.MipCount > 0U && desc.Format > 0U;
        }

        auto MatchesRegistryDesc(const FCubeMapBlobDesc& blobDesc, const FCubeMapDesc& desc,
            bool srgb) noexcept -> bool {
            if (!HasCompleteCubeMapDesc(desc)) {
                return true;
            }

            return desc.Size == blobDesc.mSize && desc.MipCount == blobDesc.mMipCount
                && desc.Format == blobDesc.mFormat && desc.SRGB == srgb;
        }

        auto ComputeTightlyPackedSize(
            const FCubeMapBlobDesc& blobDesc, u32 bytesPerPixel, u64& outSize) noexcept -> bool {
            if (bytesPerPixel == 0U) {
                return false;
            }

            u32 size = blobDesc.mSize;
            if (size == 0U || blobDesc.mMipCount == 0U) {
                return false;
            }

            u64 total = 0;
            for (u32 mip = 0; mip < blobDesc.mMipCount; ++mip) {
                const u64 rowPitch    = static_cast<u64>(size) * static_cast<u64>(bytesPerPixel);
                const u64 faceMipSize = rowPitch * static_cast<u64>(size);
                if (rowPitch == 0U || faceMipSize / rowPitch != size) {
                    return false;
                }

                const u64 mipSize = faceMipSize * static_cast<u64>(kCubeMapFaceCount);
                if (kCubeMapFaceCount == 0U
                    || mipSize / static_cast<u64>(kCubeMapFaceCount) != faceMipSize) {
                    return false;
                }

                if (total > TNumericProperty<u64>::Max - mipSize) {
                    return false;
                }
                total += mipSize;

                size = (size > 1U) ? (size >> 1U) : 1U;
            }

            outSize = total;
            return true;
        }

        auto ReadHeader(IAssetStream& stream, FAssetBlobHeader& outHeader) -> bool {
            if (!ReadExact(stream, &outHeader, sizeof(FAssetBlobHeader))) {
                return false;
            }

            if (outHeader.mMagic != kAssetBlobMagic || outHeader.mVersion != kAssetBlobVersion) {
                return false;
            }

            if (outHeader.mType != static_cast<u8>(EAssetType::CubeMap)) {
                return false;
            }

            if (outHeader.mDescSize != sizeof(FCubeMapBlobDesc)) {
                return false;
            }

            return true;
        }

        template <typename TDerived, typename... Args>
        auto MakeSharedAsset(Args&&... args) -> TShared<IAsset> {
            return Container::MakeSharedAs<IAsset, TDerived>(Forward<Args>(args)...);
        }
    } // namespace

    auto FCubeMapLoader::CanLoad(EAssetType type) const noexcept -> bool {
        return type == EAssetType::CubeMap;
    }

    auto FCubeMapLoader::Load(const FAssetDesc& desc, IAssetStream& stream) -> TShared<IAsset> {
        FAssetBlobHeader header{};
        if (!ReadHeader(stream, header)) {
            return {};
        }

        FCubeMapBlobDesc blobDesc{};
        if (!ReadExact(stream, &blobDesc, sizeof(FCubeMapBlobDesc))) {
            return {};
        }

        const u32 bytesPerPixel = GetTextureBytesPerPixel(blobDesc.mFormat);
        if (bytesPerPixel == 0U || blobDesc.mSize == 0U || blobDesc.mMipCount == 0U) {
            return {};
        }

        const u64 minRowPitch = static_cast<u64>(blobDesc.mSize) * static_cast<u64>(bytesPerPixel);
        if (static_cast<u64>(blobDesc.mRowPitch) != minRowPitch) {
            return {};
        }

        u64 expectedSize = 0;
        if (!ComputeTightlyPackedSize(blobDesc, bytesPerPixel, expectedSize)) {
            return {};
        }
        if (expectedSize > static_cast<u64>(TNumericProperty<usize>::Max)) {
            return {};
        }

        if (static_cast<u64>(header.mDataSize) != expectedSize) {
            return {};
        }

        const bool srgb = HasAssetBlobFlag(header.mFlags, EAssetBlobFlags::SRGB);
        if (!MatchesRegistryDesc(blobDesc, desc.mCubeMap, srgb)) {
            return {};
        }

        TVector<u8> pixels;
        pixels.Resize(static_cast<usize>(header.mDataSize));
        if (header.mDataSize > 0 && !ReadExact(stream, pixels.Data(), header.mDataSize)) {
            return {};
        }

        FCubeMapDesc cubeDesc{};
        cubeDesc.Size     = blobDesc.mSize;
        cubeDesc.MipCount = blobDesc.mMipCount;
        cubeDesc.Format   = blobDesc.mFormat;
        cubeDesc.SRGB     = srgb;

        return MakeSharedAsset<FCubeMapAsset>(cubeDesc, Move(pixels));
    }
} // namespace AltinaEngine::Asset
