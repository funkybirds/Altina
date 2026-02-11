#include "Asset/Texture2DLoader.h"

#include "Asset/AssetBinary.h"
#include "Asset/Texture2DAsset.h"
#include "Types/Traits.h"

#include <cstring>
#include <limits>

namespace AltinaEngine::Asset {
    namespace {
        auto ReadExact(IAssetStream& stream, void* outBuffer, usize size) -> bool {
            if (outBuffer == nullptr || size == 0U) {
                return false;
            }

            auto*       out = static_cast<u8*>(outBuffer);
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
            using Core::Container::kSmartPtrUseManagedAllocator;
            using Core::Container::TAllocator;
            using Core::Container::TAllocatorTraits;
            using Core::Container::TPolymorphicDeleter;

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

            return TShared<IAsset>(ptr,
                TPolymorphicDeleter<IAsset>(&Core::Container::DestroyPolymorphic<IAsset, TDerived>));
        }
    } // namespace

    auto FTexture2DLoader::CanLoad(EAssetType type) const noexcept -> bool {
        return type == EAssetType::Texture2D;
    }

    auto FTexture2DLoader::Load(const FAssetDesc& desc, IAssetStream& stream) -> TShared<IAsset> {
        (void)desc;

        FAssetBlobHeader header{};
        if (!ReadHeader(stream, header)) {
            return {};
        }

        FTexture2DBlobDesc blobDesc{};
        if (!ReadExact(stream, &blobDesc, sizeof(FTexture2DBlobDesc))) {
            return {};
        }

        const u32 bytesPerPixel = GetTextureBytesPerPixel(blobDesc.Format);
        if (bytesPerPixel == 0 || blobDesc.Width == 0 || blobDesc.Height == 0) {
            return {};
        }

        const u64 minRowPitch = static_cast<u64>(blobDesc.Width) * bytesPerPixel;
        if (blobDesc.RowPitch < minRowPitch) {
            return {};
        }

        const u64 expectedSize = static_cast<u64>(blobDesc.RowPitch) * blobDesc.Height;
        if (expectedSize > static_cast<u64>(std::numeric_limits<usize>::max())) {
            return {};
        }

        if (header.DataSize < expectedSize) {
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
        textureDesc.SRGB     = HasAssetBlobFlag(header.Flags, EAssetBlobFlags::SRGB);

        return MakeSharedAsset<FTexture2DAsset>(textureDesc, AltinaEngine::Move(pixels));
    }

} // namespace AltinaEngine::Asset
