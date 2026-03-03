#include "Asset/CubeMapLoader.h"

#include "Asset/AssetBinary.h"
#include "Asset/CubeMapAsset.h"

#include <cstring>
#include <limits>

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

            return desc.Size == blobDesc.Size && desc.MipCount == blobDesc.MipCount
                && desc.Format == blobDesc.Format && desc.SRGB == srgb;
        }

        auto ComputeTightlyPackedSize(
            const FCubeMapBlobDesc& blobDesc, u32 bytesPerPixel, u64& outSize) noexcept -> bool {
            if (bytesPerPixel == 0U) {
                return false;
            }

            u32 size = blobDesc.Size;
            if (size == 0U || blobDesc.MipCount == 0U) {
                return false;
            }

            u64 total = 0;
            for (u32 mip = 0; mip < blobDesc.MipCount; ++mip) {
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

                if (total > std::numeric_limits<u64>::max() - mipSize) {
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

            if (outHeader.Magic != kAssetBlobMagic || outHeader.Version != kAssetBlobVersion) {
                return false;
            }

            if (outHeader.Type != static_cast<u8>(EAssetType::CubeMap)) {
                return false;
            }

            if (outHeader.DescSize != sizeof(FCubeMapBlobDesc)) {
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

        const u32 bytesPerPixel = GetTextureBytesPerPixel(blobDesc.Format);
        if (bytesPerPixel == 0U || blobDesc.Size == 0U || blobDesc.MipCount == 0U) {
            return {};
        }

        const u64 minRowPitch = static_cast<u64>(blobDesc.Size) * static_cast<u64>(bytesPerPixel);
        if (static_cast<u64>(blobDesc.RowPitch) != minRowPitch) {
            return {};
        }

        u64 expectedSize = 0;
        if (!ComputeTightlyPackedSize(blobDesc, bytesPerPixel, expectedSize)) {
            return {};
        }
        if (expectedSize > static_cast<u64>(std::numeric_limits<usize>::max())) {
            return {};
        }

        if (static_cast<u64>(header.DataSize) != expectedSize) {
            return {};
        }

        const bool srgb = HasAssetBlobFlag(header.Flags, EAssetBlobFlags::SRGB);
        if (!MatchesRegistryDesc(blobDesc, desc.CubeMap, srgb)) {
            return {};
        }

        TVector<u8> pixels;
        pixels.Resize(static_cast<usize>(header.DataSize));
        if (header.DataSize > 0 && !ReadExact(stream, pixels.Data(), header.DataSize)) {
            return {};
        }

        FCubeMapDesc cubeDesc{};
        cubeDesc.Size     = blobDesc.Size;
        cubeDesc.MipCount = blobDesc.MipCount;
        cubeDesc.Format   = blobDesc.Format;
        cubeDesc.SRGB     = srgb;

        return MakeSharedAsset<FCubeMapAsset>(cubeDesc, Move(pixels));
    }
} // namespace AltinaEngine::Asset
