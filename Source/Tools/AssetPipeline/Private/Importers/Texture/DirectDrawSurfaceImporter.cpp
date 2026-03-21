#include "Importers/Texture/DirectDrawSurfaceImporter.h"

#include "Asset/AssetBinary.h"

#include <cstring>
#include <limits>

namespace AltinaEngine::Tools::AssetPipeline {
    namespace {
        constexpr u32 kDdsMagic = 0x20534444U;

        constexpr u32 MakeFourCc(char a, char b, char c, char d) noexcept {
            return static_cast<u32>(static_cast<u8>(a))
                | (static_cast<u32>(static_cast<u8>(b)) << 8U)
                | (static_cast<u32>(static_cast<u8>(c)) << 16U)
                | (static_cast<u32>(static_cast<u8>(d)) << 24U);
        }

        constexpr u32 kDdsPixelFormatFourCc = 0x4U;
        constexpr u32 kDdsCaps2CubeMap      = 0x00000200U;
        constexpr u32 kD3d10ResourceTex2D   = 3U;

        constexpr u32 kDxgiBc1Typeless  = 70U;
        constexpr u32 kDxgiBc1Unorm     = 71U;
        constexpr u32 kDxgiBc1UnormSrgb = 72U;
        constexpr u32 kDxgiBc2Typeless  = 73U;
        constexpr u32 kDxgiBc2Unorm     = 74U;
        constexpr u32 kDxgiBc2UnormSrgb = 75U;
        constexpr u32 kDxgiBc3Typeless  = 76U;
        constexpr u32 kDxgiBc3Unorm     = 77U;
        constexpr u32 kDxgiBc3UnormSrgb = 78U;
        constexpr u32 kDxgiBc4Typeless  = 79U;
        constexpr u32 kDxgiBc4Unorm     = 80U;
        constexpr u32 kDxgiBc5Typeless  = 82U;
        constexpr u32 kDxgiBc5Unorm     = 83U;

        struct FDdsPixelFormat {
            u32 mSize        = 0U;
            u32 mFlags       = 0U;
            u32 mFourCc      = 0U;
            u32 mRgbBitCount = 0U;
            u32 mRBitMask    = 0U;
            u32 mGBitMask    = 0U;
            u32 mBBitMask    = 0U;
            u32 mABitMask    = 0U;
        };

        struct FDdsHeader {
            u32             mSize              = 0U;
            u32             mFlags             = 0U;
            u32             mHeight            = 0U;
            u32             mWidth             = 0U;
            u32             mPitchOrLinearSize = 0U;
            u32             mDepth             = 0U;
            u32             mMipMapCount       = 0U;
            u32             mReserved1[11]     = {};
            FDdsPixelFormat mPixelFormat       = {};
            u32             mCaps              = 0U;
            u32             mCaps2             = 0U;
            u32             mCaps3             = 0U;
            u32             mCaps4             = 0U;
            u32             mReserved2         = 0U;
        };

        struct FDdsHeaderDx10 {
            u32 mDxgiFormat        = 0U;
            u32 mResourceDimension = 0U;
            u32 mMiscFlag          = 0U;
            u32 mArraySize         = 0U;
            u32 mMiscFlags2        = 0U;
        };

        struct FDdsParseResult {
            Asset::FTexture2DBlobDesc BlobDesc{};
            const u8*                 PixelData     = nullptr;
            usize                     PixelDataSize = 0U;
            bool                      Srgb          = false;
        };

        auto WriteCookedTextureBlob(const Asset::FTexture2DBlobDesc& blobDesc, const u8* data,
            usize dataSize, bool srgb, std::vector<u8>& outCooked, Asset::FTexture2DDesc& outDesc)
            -> bool {
            if (data == nullptr || dataSize == 0U
                || dataSize > static_cast<usize>(std::numeric_limits<u32>::max())) {
                return false;
            }

            Asset::FAssetBlobHeader header{};
            header.mType     = static_cast<u8>(Asset::EAssetType::Texture2D);
            header.mFlags    = Asset::MakeAssetBlobFlags(srgb);
            header.mDescSize = static_cast<u32>(sizeof(Asset::FTexture2DBlobDesc));
            header.mDataSize = static_cast<u32>(dataSize);

            const usize totalSize =
                sizeof(Asset::FAssetBlobHeader) + sizeof(Asset::FTexture2DBlobDesc) + dataSize;
            outCooked.resize(totalSize);
            std::memcpy(outCooked.data(), &header, sizeof(header));
            std::memcpy(outCooked.data() + sizeof(header), &blobDesc, sizeof(blobDesc));
            std::memcpy(outCooked.data() + sizeof(header) + sizeof(blobDesc), data, dataSize);

            outDesc.Width    = blobDesc.mWidth;
            outDesc.Height   = blobDesc.mHeight;
            outDesc.MipCount = blobDesc.mMipCount;
            outDesc.Format   = blobDesc.mFormat;
            outDesc.SRGB     = srgb;
            return true;
        }

        auto ResolveLegacyFormat(u32 fourCc, u32& outFormat) -> bool {
            switch (fourCc) {
                case MakeFourCc('D', 'X', 'T', '1'):
                    outFormat = Asset::kTextureFormatBC1;
                    return true;
                case MakeFourCc('D', 'X', 'T', '3'):
                case MakeFourCc('D', 'X', 'T', '2'):
                    outFormat = Asset::kTextureFormatBC2;
                    return true;
                case MakeFourCc('D', 'X', 'T', '5'):
                case MakeFourCc('D', 'X', 'T', '4'):
                    outFormat = Asset::kTextureFormatBC3;
                    return true;
                case MakeFourCc('A', 'T', 'I', '1'):
                case MakeFourCc('B', 'C', '4', 'U'):
                    outFormat = Asset::kTextureFormatBC4;
                    return true;
                case MakeFourCc('A', 'T', 'I', '2'):
                case MakeFourCc('B', 'C', '5', 'U'):
                    outFormat = Asset::kTextureFormatBC5;
                    return true;
                default:
                    outFormat = Asset::kTextureFormatUnknown;
                    return false;
            }
        }

        auto ResolveDx10Format(u32 dxgiFormat, u32& outFormat, bool& outSrgb) -> bool {
            outSrgb = false;
            switch (dxgiFormat) {
                case kDxgiBc1Typeless:
                case kDxgiBc1Unorm:
                    outFormat = Asset::kTextureFormatBC1;
                    return true;
                case kDxgiBc1UnormSrgb:
                    outFormat = Asset::kTextureFormatBC1;
                    outSrgb   = true;
                    return true;
                case kDxgiBc2Typeless:
                case kDxgiBc2Unorm:
                    outFormat = Asset::kTextureFormatBC2;
                    return true;
                case kDxgiBc2UnormSrgb:
                    outFormat = Asset::kTextureFormatBC2;
                    outSrgb   = true;
                    return true;
                case kDxgiBc3Typeless:
                case kDxgiBc3Unorm:
                    outFormat = Asset::kTextureFormatBC3;
                    return true;
                case kDxgiBc3UnormSrgb:
                    outFormat = Asset::kTextureFormatBC3;
                    outSrgb   = true;
                    return true;
                case kDxgiBc4Typeless:
                case kDxgiBc4Unorm:
                    outFormat = Asset::kTextureFormatBC4;
                    return true;
                case kDxgiBc5Typeless:
                case kDxgiBc5Unorm:
                    outFormat = Asset::kTextureFormatBC5;
                    return true;
                default:
                    outFormat = Asset::kTextureFormatUnknown;
                    return false;
            }
        }

        auto ComputePixelPayloadSize(const Asset::FTexture2DBlobDesc& blobDesc, usize& outSize)
            -> bool {
            if (blobDesc.mWidth == 0U || blobDesc.mHeight == 0U || blobDesc.mMipCount == 0U) {
                return false;
            }

            const u32 bytesPerPixel = Asset::GetTextureBytesPerPixel(blobDesc.mFormat);
            if (bytesPerPixel == 0U && !Asset::IsTextureBlockCompressed(blobDesc.mFormat)) {
                return false;
            }

            const u32 expectedRowPitch =
                Asset::GetTextureMipRowPitch(blobDesc.mFormat, blobDesc.mWidth, bytesPerPixel);
            if (blobDesc.mRowPitch != expectedRowPitch) {
                return false;
            }

            u32   width  = blobDesc.mWidth;
            u32   height = blobDesc.mHeight;
            usize total  = 0U;
            for (u32 mip = 0U; mip < blobDesc.mMipCount; ++mip) {
                const u32 slicePitch =
                    Asset::GetTextureMipSlicePitch(blobDesc.mFormat, width, height, bytesPerPixel);
                if (slicePitch == 0U) {
                    return false;
                }
                if (total > std::numeric_limits<usize>::max() - static_cast<usize>(slicePitch)) {
                    return false;
                }
                total += static_cast<usize>(slicePitch);
                width  = (width > 1U) ? (width >> 1U) : 1U;
                height = (height > 1U) ? (height >> 1U) : 1U;
            }

            outSize = total;
            return true;
        }

        auto ParseDds(const std::vector<u8>& sourceBytes, bool defaultSrgb, FDdsParseResult& out)
            -> bool {
            out = {};
            if (sourceBytes.size() < sizeof(u32) + sizeof(FDdsHeader)) {
                return false;
            }

            u32 magic = 0U;
            std::memcpy(&magic, sourceBytes.data(), sizeof(magic));
            if (magic != kDdsMagic) {
                return false;
            }

            FDdsHeader header{};
            std::memcpy(&header, sourceBytes.data() + sizeof(u32), sizeof(header));
            if (header.mSize != sizeof(FDdsHeader)
                || header.mPixelFormat.mSize != sizeof(FDdsPixelFormat)) {
                return false;
            }
            if (header.mWidth == 0U || header.mHeight == 0U) {
                return false;
            }
            if (header.mDepth > 1U || (header.mCaps2 & kDdsCaps2CubeMap) != 0U) {
                return false;
            }

            Asset::FTexture2DBlobDesc blobDesc{};
            blobDesc.mWidth    = header.mWidth;
            blobDesc.mHeight   = header.mHeight;
            blobDesc.mMipCount = (header.mMipMapCount > 0U) ? header.mMipMapCount : 1U;

            usize payloadOffset = sizeof(u32) + sizeof(FDdsHeader);
            bool  srgb          = defaultSrgb;

            if ((header.mPixelFormat.mFlags & kDdsPixelFormatFourCc) == 0U) {
                return false;
            }

            if (header.mPixelFormat.mFourCc == MakeFourCc('D', 'X', '1', '0')) {
                if (sourceBytes.size() < payloadOffset + sizeof(FDdsHeaderDx10)) {
                    return false;
                }
                FDdsHeaderDx10 headerDx10{};
                std::memcpy(&headerDx10, sourceBytes.data() + payloadOffset, sizeof(headerDx10));
                payloadOffset += sizeof(FDdsHeaderDx10);

                if (headerDx10.mResourceDimension != kD3d10ResourceTex2D
                    || headerDx10.mArraySize != 1U) {
                    return false;
                }

                bool dx10Srgb = false;
                if (!ResolveDx10Format(headerDx10.mDxgiFormat, blobDesc.mFormat, dx10Srgb)) {
                    return false;
                }
                srgb = srgb || dx10Srgb;
            } else if (!ResolveLegacyFormat(header.mPixelFormat.mFourCc, blobDesc.mFormat)) {
                return false;
            }

            blobDesc.mRowPitch =
                Asset::GetTextureMipRowPitch(blobDesc.mFormat, blobDesc.mWidth, 0U);
            usize pixelDataSize = 0U;
            if (!ComputePixelPayloadSize(blobDesc, pixelDataSize)) {
                return false;
            }

            if (sourceBytes.size() < payloadOffset + pixelDataSize) {
                return false;
            }

            out.BlobDesc      = blobDesc;
            out.PixelData     = sourceBytes.data() + payloadOffset;
            out.PixelDataSize = pixelDataSize;
            out.Srgb          = srgb;
            return true;
        }

        auto BuildSelfTestDds() -> std::vector<u8> {
            FDdsHeader header{};
            header.mSize                = sizeof(FDdsHeader);
            header.mFlags               = 0x00021007U;
            header.mHeight              = 8U;
            header.mWidth               = 8U;
            header.mPitchOrLinearSize   = 32U;
            header.mMipMapCount         = 2U;
            header.mPixelFormat.mSize   = sizeof(FDdsPixelFormat);
            header.mPixelFormat.mFlags  = kDdsPixelFormatFourCc;
            header.mPixelFormat.mFourCc = MakeFourCc('D', 'X', 'T', '1');
            header.mCaps                = 0x00401008U;

            constexpr u32   kPayloadSize = 40U;
            std::vector<u8> bytes(sizeof(u32) + sizeof(FDdsHeader) + kPayloadSize, 0U);
            const u32       magic = kDdsMagic;
            std::memcpy(bytes.data(), &magic, sizeof(magic));
            std::memcpy(bytes.data() + sizeof(magic), &header, sizeof(header));
            for (u32 index = 0U; index < kPayloadSize; ++index) {
                bytes[sizeof(u32) + sizeof(FDdsHeader) + index] = static_cast<u8>(index * 7U + 3U);
            }
            return bytes;
        }
    } // namespace

    auto CookDirectDrawSurfaceTexture2D(const std::vector<u8>& sourceBytes, bool srgb,
        std::vector<u8>& outCooked, Asset::FTexture2DDesc& outDesc) -> bool {
        FDdsParseResult parsed{};
        if (!ParseDds(sourceBytes, srgb, parsed)) {
            return false;
        }
        return WriteCookedTextureBlob(parsed.BlobDesc, parsed.PixelData, parsed.PixelDataSize,
            parsed.Srgb, outCooked, outDesc);
    }

    auto RunDirectDrawSurfaceSelfTest(std::string& outError) -> bool {
        outError.clear();

        const std::vector<u8> ddsBytes = BuildSelfTestDds();
        std::vector<u8>       cookedA;
        std::vector<u8>       cookedB;
        Asset::FTexture2DDesc descA{};
        Asset::FTexture2DDesc descB{};

        if (!CookDirectDrawSurfaceTexture2D(ddsBytes, false, cookedA, descA)
            || !CookDirectDrawSurfaceTexture2D(ddsBytes, false, cookedB, descB)) {
            outError = "DDS self test: cook failed.";
            return false;
        }

        if (descA.Width != 8U || descA.Height != 8U || descA.MipCount != 2U
            || descA.Format != Asset::kTextureFormatBC1 || descA.SRGB) {
            outError = "DDS self test: unexpected output texture desc.";
            return false;
        }

        if (descA.Width != descB.Width || descA.Height != descB.Height
            || descA.MipCount != descB.MipCount || descA.Format != descB.Format
            || descA.SRGB != descB.SRGB || cookedA != cookedB) {
            outError = "DDS self test: output is not deterministic.";
            return false;
        }

        return true;
    }
} // namespace AltinaEngine::Tools::AssetPipeline
