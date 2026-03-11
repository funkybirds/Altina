#include "Asset/Texture2DLoader.h"

#include "Asset/AssetBinary.h"
#include "Asset/Texture2DAsset.h"
#include "Types/NumericProperties.h"
#include "Types/Traits.h"

#include <cstring>

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

        auto HasCompleteTextureDesc(const FTexture2DDesc& desc) noexcept -> bool {
            return desc.Width > 0U && desc.Height > 0U && desc.MipCount > 0U && desc.Format > 0U;
        }

        auto MatchesRegistryDesc(const FTexture2DBlobDesc& blobDesc, const FTexture2DDesc& desc,
            bool srgb) noexcept -> bool {
            if (!HasCompleteTextureDesc(desc)) {
                return true;
            }

            return desc.Width == blobDesc.mWidth && desc.Height == blobDesc.mHeight
                && desc.MipCount == blobDesc.mMipCount && desc.Format == blobDesc.mFormat
                && desc.SRGB == srgb;
        }

        auto ComputeTightlyPackedSize(
            const FTexture2DBlobDesc& blobDesc, u32 bytesPerPixel, u64& outSize) noexcept -> bool {
            if (bytesPerPixel == 0U) {
                return false;
            }

            u32 width  = blobDesc.mWidth;
            u32 height = blobDesc.mHeight;
            if (width == 0U || height == 0U || blobDesc.mMipCount == 0U) {
                return false;
            }

            u64 total = 0;
            for (u32 mip = 0; mip < blobDesc.mMipCount; ++mip) {
                const u64 rowPitch = static_cast<u64>(width) * bytesPerPixel;
                const u64 mipSize  = rowPitch * static_cast<u64>(height);
                if (rowPitch == 0U || mipSize / rowPitch != height) {
                    return false;
                }
                if (total > TNumericProperty<u64>::Max - mipSize) {
                    return false;
                }
                total += mipSize;

                width  = (width > 1U) ? (width >> 1U) : 1U;
                height = (height > 1U) ? (height >> 1U) : 1U;
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

            if (outHeader.mType != static_cast<u8>(EAssetType::Texture2D)) {
                return false;
            }

            if (outHeader.mDescSize != sizeof(FTexture2DBlobDesc)) {
                return false;
            }

            return true;
        }

        template <typename TDerived, typename... Args>
        auto MakeSharedAsset(Args&&... args) -> TShared<IAsset> {
            return Container::MakeSharedAs<IAsset, TDerived>(Forward<Args>(args)...);
        }
    } // namespace

    auto FTexture2DLoader::CanLoad(EAssetType type) const noexcept -> bool {
        return type == EAssetType::Texture2D;
    }

    auto FTexture2DLoader::Load(const FAssetDesc& desc, IAssetStream& stream) -> TShared<IAsset> {
        FAssetBlobHeader header{};
        if (!ReadHeader(stream, header)) {
            return {};
        }

        FTexture2DBlobDesc blobDesc{};
        if (!ReadExact(stream, &blobDesc, sizeof(FTexture2DBlobDesc))) {
            return {};
        }

        const u32 bytesPerPixel = GetTextureBytesPerPixel(blobDesc.mFormat);
        if (bytesPerPixel == 0 || blobDesc.mWidth == 0 || blobDesc.mHeight == 0
            || blobDesc.mMipCount == 0) {
            return {};
        }

        const u64 minRowPitch = static_cast<u64>(blobDesc.mWidth) * bytesPerPixel;
        if (blobDesc.mRowPitch != minRowPitch) {
            return {};
        }

        u64 expectedSize = 0;
        if (!ComputeTightlyPackedSize(blobDesc, bytesPerPixel, expectedSize)) {
            return {};
        }
        if (expectedSize > static_cast<u64>(TNumericProperty<usize>::Max)) {
            return {};
        }

        if (header.mDataSize != expectedSize) {
            return {};
        }

        const bool srgb = HasAssetBlobFlag(header.mFlags, EAssetBlobFlags::SRGB);
        if (!MatchesRegistryDesc(blobDesc, desc.mTexture, srgb)) {
            return {};
        }

        TVector<u8> pixels;
        pixels.Resize(static_cast<usize>(header.mDataSize));
        if (header.mDataSize > 0 && !ReadExact(stream, pixels.Data(), header.mDataSize)) {
            return {};
        }

        FTexture2DDesc textureDesc{};
        textureDesc.Width    = blobDesc.mWidth;
        textureDesc.Height   = blobDesc.mHeight;
        textureDesc.MipCount = blobDesc.mMipCount;
        textureDesc.Format   = blobDesc.mFormat;
        textureDesc.SRGB     = srgb;

        return MakeSharedAsset<FTexture2DAsset>(textureDesc, Move(pixels));
    }

} // namespace AltinaEngine::Asset
