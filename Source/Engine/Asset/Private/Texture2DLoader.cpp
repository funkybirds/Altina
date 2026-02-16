#include "Asset/Texture2DLoader.h"

#include "Asset/AssetBinary.h"
#include "Asset/Texture2DAsset.h"
#include "Types/Traits.h"

#include <cstring>
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

        auto HasCompleteTextureDesc(const FTexture2DDesc& desc) noexcept -> bool {
            return desc.Width > 0U && desc.Height > 0U && desc.MipCount > 0U && desc.Format > 0U;
        }

        auto MatchesRegistryDesc(const FTexture2DBlobDesc& blobDesc, const FTexture2DDesc& desc,
            bool srgb) noexcept -> bool {
            if (!HasCompleteTextureDesc(desc)) {
                return true;
            }

            return desc.Width == blobDesc.Width && desc.Height == blobDesc.Height
                && desc.MipCount == blobDesc.MipCount && desc.Format == blobDesc.Format
                && desc.SRGB == srgb;
        }

        auto ComputeTightlyPackedSize(
            const FTexture2DBlobDesc& blobDesc, u32 bytesPerPixel, u64& outSize) noexcept -> bool {
            if (bytesPerPixel == 0U) {
                return false;
            }

            u32 width  = blobDesc.Width;
            u32 height = blobDesc.Height;
            if (width == 0U || height == 0U || blobDesc.MipCount == 0U) {
                return false;
            }

            u64 total = 0;
            for (u32 mip = 0; mip < blobDesc.MipCount; ++mip) {
                const u64 rowPitch = static_cast<u64>(width) * bytesPerPixel;
                const u64 mipSize  = rowPitch * static_cast<u64>(height);
                if (rowPitch == 0U || mipSize / rowPitch != height) {
                    return false;
                }
                if (total > std::numeric_limits<u64>::max() - mipSize) {
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

            if (outHeader.Magic != kAssetBlobMagic || outHeader.Version != kAssetBlobVersion) {
                return false;
            }

            if (outHeader.Type != static_cast<u8>(EAssetType::Texture2D)) {
                return false;
            }

            if (outHeader.DescSize != sizeof(FTexture2DBlobDesc)) {
                return false;
            }

            return true;
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

        const u32 bytesPerPixel = GetTextureBytesPerPixel(blobDesc.Format);
        if (bytesPerPixel == 0 || blobDesc.Width == 0 || blobDesc.Height == 0
            || blobDesc.MipCount == 0) {
            return {};
        }

        const u64 minRowPitch = static_cast<u64>(blobDesc.Width) * bytesPerPixel;
        if (blobDesc.RowPitch != minRowPitch) {
            return {};
        }

        u64 expectedSize = 0;
        if (!ComputeTightlyPackedSize(blobDesc, bytesPerPixel, expectedSize)) {
            return {};
        }
        if (expectedSize > static_cast<u64>(std::numeric_limits<usize>::max())) {
            return {};
        }

        if (header.DataSize != expectedSize) {
            return {};
        }

        const bool srgb = HasAssetBlobFlag(header.Flags, EAssetBlobFlags::SRGB);
        if (!MatchesRegistryDesc(blobDesc, desc.Texture, srgb)) {
            return {};
        }

        TVector<u8> pixels;
        pixels.Resize(static_cast<usize>(header.DataSize));
        if (header.DataSize > 0 && !ReadExact(stream, pixels.Data(), header.DataSize)) {
            return {};
        }

        FTexture2DDesc textureDesc{};
        textureDesc.Width    = blobDesc.Width;
        textureDesc.Height   = blobDesc.Height;
        textureDesc.MipCount = blobDesc.MipCount;
        textureDesc.Format   = blobDesc.Format;
        textureDesc.SRGB     = srgb;

        return MakeSharedAsset<FTexture2DAsset>(textureDesc, Move(pixels));
    }

} // namespace AltinaEngine::Asset
