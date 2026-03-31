#include "Asset/Texture2DAsset.h"

#include "Asset/AssetBinary.h"
#include "Asset/TextureDecode.h"

#include <cmath>

#include "Types/Traits.h"

using AltinaEngine::Move;
namespace AltinaEngine::Asset {
    namespace {
        [[nodiscard]] constexpr auto Expand5To8(u8 value) noexcept -> u8 {
            return static_cast<u8>((value << 3U) | (value >> 2U));
        }

        [[nodiscard]] constexpr auto Expand6To8(u8 value) noexcept -> u8 {
            return static_cast<u8>((value << 2U) | (value >> 4U));
        }

        void DecodeBc565Color(u16 packed, u8& outR, u8& outG, u8& outB) noexcept {
            outR = Expand5To8(static_cast<u8>((packed >> 11U) & 0x1FU));
            outG = Expand6To8(static_cast<u8>((packed >> 5U) & 0x3FU));
            outB = Expand5To8(static_cast<u8>(packed & 0x1FU));
        }

        void BuildBc1Palette(
            const u8* block, bool allowPunchThrough, u8 outPalette[4][4]) noexcept {
            const u16 color0 = static_cast<u16>(block[0]) | (static_cast<u16>(block[1]) << 8U);
            const u16 color1 = static_cast<u16>(block[2]) | (static_cast<u16>(block[3]) << 8U);

            DecodeBc565Color(color0, outPalette[0][0], outPalette[0][1], outPalette[0][2]);
            DecodeBc565Color(color1, outPalette[1][0], outPalette[1][1], outPalette[1][2]);
            outPalette[0][3] = 255U;
            outPalette[1][3] = 255U;

            const bool useThreeColor = allowPunchThrough && (color0 <= color1);
            if (useThreeColor) {
                outPalette[2][0] = static_cast<u8>(
                    (static_cast<u32>(outPalette[0][0]) + static_cast<u32>(outPalette[1][0])) / 2U);
                outPalette[2][1] = static_cast<u8>(
                    (static_cast<u32>(outPalette[0][1]) + static_cast<u32>(outPalette[1][1])) / 2U);
                outPalette[2][2] = static_cast<u8>(
                    (static_cast<u32>(outPalette[0][2]) + static_cast<u32>(outPalette[1][2])) / 2U);
                outPalette[2][3] = 255U;
                outPalette[3][0] = 0U;
                outPalette[3][1] = 0U;
                outPalette[3][2] = 0U;
                outPalette[3][3] = 0U;
                return;
            }

            outPalette[2][0] = static_cast<u8>(
                (2U * static_cast<u32>(outPalette[0][0]) + static_cast<u32>(outPalette[1][0]))
                / 3U);
            outPalette[2][1] = static_cast<u8>(
                (2U * static_cast<u32>(outPalette[0][1]) + static_cast<u32>(outPalette[1][1]))
                / 3U);
            outPalette[2][2] = static_cast<u8>(
                (2U * static_cast<u32>(outPalette[0][2]) + static_cast<u32>(outPalette[1][2]))
                / 3U);
            outPalette[2][3] = 255U;

            outPalette[3][0] = static_cast<u8>(
                (static_cast<u32>(outPalette[0][0]) + 2U * static_cast<u32>(outPalette[1][0]))
                / 3U);
            outPalette[3][1] = static_cast<u8>(
                (static_cast<u32>(outPalette[0][1]) + 2U * static_cast<u32>(outPalette[1][1]))
                / 3U);
            outPalette[3][2] = static_cast<u8>(
                (static_cast<u32>(outPalette[0][2]) + 2U * static_cast<u32>(outPalette[1][2]))
                / 3U);
            outPalette[3][3] = 255U;
        }

        void BuildBcAlphaPalette(u8 endpoint0, u8 endpoint1, u8 outPalette[8]) noexcept {
            outPalette[0] = endpoint0;
            outPalette[1] = endpoint1;
            if (endpoint0 > endpoint1) {
                outPalette[2] = static_cast<u8>(
                    (6U * static_cast<u32>(endpoint0) + static_cast<u32>(endpoint1)) / 7U);
                outPalette[3] = static_cast<u8>(
                    (5U * static_cast<u32>(endpoint0) + 2U * static_cast<u32>(endpoint1)) / 7U);
                outPalette[4] = static_cast<u8>(
                    (4U * static_cast<u32>(endpoint0) + 3U * static_cast<u32>(endpoint1)) / 7U);
                outPalette[5] = static_cast<u8>(
                    (3U * static_cast<u32>(endpoint0) + 4U * static_cast<u32>(endpoint1)) / 7U);
                outPalette[6] = static_cast<u8>(
                    (2U * static_cast<u32>(endpoint0) + 5U * static_cast<u32>(endpoint1)) / 7U);
                outPalette[7] = static_cast<u8>(
                    (static_cast<u32>(endpoint0) + 6U * static_cast<u32>(endpoint1)) / 7U);
                return;
            }

            outPalette[2] = static_cast<u8>(
                (4U * static_cast<u32>(endpoint0) + static_cast<u32>(endpoint1)) / 5U);
            outPalette[3] = static_cast<u8>(
                (3U * static_cast<u32>(endpoint0) + 2U * static_cast<u32>(endpoint1)) / 5U);
            outPalette[4] = static_cast<u8>(
                (2U * static_cast<u32>(endpoint0) + 3U * static_cast<u32>(endpoint1)) / 5U);
            outPalette[5] = static_cast<u8>(
                (static_cast<u32>(endpoint0) + 4U * static_cast<u32>(endpoint1)) / 5U);
            outPalette[6] = 0U;
            outPalette[7] = 255U;
        }

        void WritePixelRgba8(
            TVector<u8>& pixels, u32 width, u32 x, u32 y, u8 r, u8 g, u8 b, u8 a) noexcept {
            const usize index =
                (static_cast<usize>(y) * static_cast<usize>(width) + static_cast<usize>(x)) * 4U;
            pixels[index + 0U] = r;
            pixels[index + 1U] = g;
            pixels[index + 2U] = b;
            pixels[index + 3U] = a;
        }

        auto DecodeBc1Mip(const u8* src, u32 width, u32 height, TVector<u8>& outPixels) -> bool {
            if (src == nullptr || width == 0U || height == 0U) {
                return false;
            }

            outPixels.Resize(static_cast<usize>(width) * static_cast<usize>(height) * 4U);
            const u32 blocksX = (width + 3U) / 4U;
            const u32 blocksY = (height + 3U) / 4U;
            for (u32 blockY = 0U; blockY < blocksY; ++blockY) {
                for (u32 blockX = 0U; blockX < blocksX; ++blockX) {
                    const u8* block = src + (static_cast<usize>(blockY) * blocksX + blockX) * 8U;
                    u8        palette[4][4]{};
                    BuildBc1Palette(block, true, palette);
                    const u32 indices = static_cast<u32>(block[4])
                        | (static_cast<u32>(block[5]) << 8U) | (static_cast<u32>(block[6]) << 16U)
                        | (static_cast<u32>(block[7]) << 24U);

                    for (u32 py = 0U; py < 4U; ++py) {
                        for (u32 px = 0U; px < 4U; ++px) {
                            const u32 dstX = blockX * 4U + px;
                            const u32 dstY = blockY * 4U + py;
                            if (dstX >= width || dstY >= height) {
                                continue;
                            }

                            const u32 code = (indices >> (2U * (py * 4U + px))) & 0x3U;
                            WritePixelRgba8(outPixels, width, dstX, dstY, palette[code][0],
                                palette[code][1], palette[code][2], palette[code][3]);
                        }
                    }
                }
            }
            return true;
        }

        auto DecodeBc2Mip(const u8* src, u32 width, u32 height, TVector<u8>& outPixels) -> bool {
            if (src == nullptr || width == 0U || height == 0U) {
                return false;
            }

            outPixels.Resize(static_cast<usize>(width) * static_cast<usize>(height) * 4U);
            const u32 blocksX = (width + 3U) / 4U;
            const u32 blocksY = (height + 3U) / 4U;
            for (u32 blockY = 0U; blockY < blocksY; ++blockY) {
                for (u32 blockX = 0U; blockX < blocksX; ++blockX) {
                    const u8* block = src + (static_cast<usize>(blockY) * blocksX + blockX) * 16U;
                    u8        palette[4][4]{};
                    BuildBc1Palette(block + 8U, false, palette);

                    u64 alphaBits = 0ULL;
                    for (u32 byteIndex = 0U; byteIndex < 8U; ++byteIndex) {
                        alphaBits |= static_cast<u64>(block[byteIndex]) << (8U * byteIndex);
                    }
                    const u32 colorIndices = static_cast<u32>(block[12])
                        | (static_cast<u32>(block[13]) << 8U) | (static_cast<u32>(block[14]) << 16U)
                        | (static_cast<u32>(block[15]) << 24U);

                    for (u32 py = 0U; py < 4U; ++py) {
                        for (u32 px = 0U; px < 4U; ++px) {
                            const u32 dstX = blockX * 4U + px;
                            const u32 dstY = blockY * 4U + py;
                            if (dstX >= width || dstY >= height) {
                                continue;
                            }

                            const u32 pixelIndex = py * 4U + px;
                            const u32 colorCode  = (colorIndices >> (2U * pixelIndex)) & 0x3U;
                            const u8  alphaNibble =
                                static_cast<u8>((alphaBits >> (4U * pixelIndex)) & 0xFU);
                            WritePixelRgba8(outPixels, width, dstX, dstY, palette[colorCode][0],
                                palette[colorCode][1], palette[colorCode][2],
                                static_cast<u8>(alphaNibble * 17U));
                        }
                    }
                }
            }
            return true;
        }

        auto DecodeBc3Mip(const u8* src, u32 width, u32 height, TVector<u8>& outPixels) -> bool {
            if (src == nullptr || width == 0U || height == 0U) {
                return false;
            }

            outPixels.Resize(static_cast<usize>(width) * static_cast<usize>(height) * 4U);
            const u32 blocksX = (width + 3U) / 4U;
            const u32 blocksY = (height + 3U) / 4U;
            for (u32 blockY = 0U; blockY < blocksY; ++blockY) {
                for (u32 blockX = 0U; blockX < blocksX; ++blockX) {
                    const u8* block = src + (static_cast<usize>(blockY) * blocksX + blockX) * 16U;
                    u8        alphaPalette[8]{};
                    BuildBcAlphaPalette(block[0], block[1], alphaPalette);

                    u64 alphaIndices = 0ULL;
                    for (u32 byteIndex = 0U; byteIndex < 6U; ++byteIndex) {
                        alphaIndices |= static_cast<u64>(block[2U + byteIndex]) << (8U * byteIndex);
                    }

                    u8 palette[4][4]{};
                    BuildBc1Palette(block + 8U, false, palette);
                    const u32 colorIndices = static_cast<u32>(block[12])
                        | (static_cast<u32>(block[13]) << 8U) | (static_cast<u32>(block[14]) << 16U)
                        | (static_cast<u32>(block[15]) << 24U);

                    for (u32 py = 0U; py < 4U; ++py) {
                        for (u32 px = 0U; px < 4U; ++px) {
                            const u32 dstX = blockX * 4U + px;
                            const u32 dstY = blockY * 4U + py;
                            if (dstX >= width || dstY >= height) {
                                continue;
                            }

                            const u32 pixelIndex = py * 4U + px;
                            const u32 colorCode  = (colorIndices >> (2U * pixelIndex)) & 0x3U;
                            const u32 alphaCode =
                                static_cast<u32>((alphaIndices >> (3U * pixelIndex)) & 0x7ULL);
                            WritePixelRgba8(outPixels, width, dstX, dstY, palette[colorCode][0],
                                palette[colorCode][1], palette[colorCode][2],
                                alphaPalette[alphaCode]);
                        }
                    }
                }
            }
            return true;
        }

        auto DecodeBc4Mip(const u8* src, u32 width, u32 height, TVector<u8>& outPixels) -> bool {
            if (src == nullptr || width == 0U || height == 0U) {
                return false;
            }

            outPixels.Resize(static_cast<usize>(width) * static_cast<usize>(height) * 4U);
            const u32 blocksX = (width + 3U) / 4U;
            const u32 blocksY = (height + 3U) / 4U;
            for (u32 blockY = 0U; blockY < blocksY; ++blockY) {
                for (u32 blockX = 0U; blockX < blocksX; ++blockX) {
                    const u8* block = src + (static_cast<usize>(blockY) * blocksX + blockX) * 8U;
                    u8        redPalette[8]{};
                    BuildBcAlphaPalette(block[0], block[1], redPalette);

                    u64 redIndices = 0ULL;
                    for (u32 byteIndex = 0U; byteIndex < 6U; ++byteIndex) {
                        redIndices |= static_cast<u64>(block[2U + byteIndex]) << (8U * byteIndex);
                    }

                    for (u32 py = 0U; py < 4U; ++py) {
                        for (u32 px = 0U; px < 4U; ++px) {
                            const u32 dstX = blockX * 4U + px;
                            const u32 dstY = blockY * 4U + py;
                            if (dstX >= width || dstY >= height) {
                                continue;
                            }

                            const u32 pixelIndex = py * 4U + px;
                            const u32 redCode =
                                static_cast<u32>((redIndices >> (3U * pixelIndex)) & 0x7ULL);
                            const u8 value = redPalette[redCode];
                            WritePixelRgba8(
                                outPixels, width, dstX, dstY, value, value, value, 255U);
                        }
                    }
                }
            }
            return true;
        }

        auto DecodeBc5Mip(const u8* src, u32 width, u32 height, TVector<u8>& outPixels) -> bool {
            if (src == nullptr || width == 0U || height == 0U) {
                return false;
            }

            outPixels.Resize(static_cast<usize>(width) * static_cast<usize>(height) * 4U);
            const u32 blocksX = (width + 3U) / 4U;
            const u32 blocksY = (height + 3U) / 4U;
            for (u32 blockY = 0U; blockY < blocksY; ++blockY) {
                for (u32 blockX = 0U; blockX < blocksX; ++blockX) {
                    const u8* block = src + (static_cast<usize>(blockY) * blocksX + blockX) * 16U;
                    u8        redPalette[8]{};
                    u8        greenPalette[8]{};
                    BuildBcAlphaPalette(block[0], block[1], redPalette);
                    BuildBcAlphaPalette(block[8], block[9], greenPalette);

                    u64 redIndices   = 0ULL;
                    u64 greenIndices = 0ULL;
                    for (u32 byteIndex = 0U; byteIndex < 6U; ++byteIndex) {
                        redIndices |= static_cast<u64>(block[2U + byteIndex]) << (8U * byteIndex);
                        greenIndices |= static_cast<u64>(block[10U + byteIndex])
                            << (8U * byteIndex);
                    }

                    for (u32 py = 0U; py < 4U; ++py) {
                        for (u32 px = 0U; px < 4U; ++px) {
                            const u32 dstX = blockX * 4U + px;
                            const u32 dstY = blockY * 4U + py;
                            if (dstX >= width || dstY >= height) {
                                continue;
                            }

                            const u32 pixelIndex = py * 4U + px;
                            const u8  redValue =
                                redPalette[(redIndices >> (3U * pixelIndex)) & 0x7ULL];
                            const u8 greenValue =
                                greenPalette[(greenIndices >> (3U * pixelIndex)) & 0x7ULL];

                            const f32 nx = static_cast<f32>(redValue) / 255.0f * 2.0f - 1.0f;
                            const f32 ny = static_cast<f32>(greenValue) / 255.0f * 2.0f - 1.0f;
                            f32       nz = 1.0f - nx * nx - ny * ny;
                            if (nz < 0.0f) {
                                nz = 0.0f;
                            }
                            nz                 = std::sqrt(nz);
                            const u8 blueValue = static_cast<u8>((nz * 0.5f + 0.5f) * 255.0f);

                            WritePixelRgba8(outPixels, width, dstX, dstY, redValue, greenValue,
                                blueValue, 255U);
                        }
                    }
                }
            }
            return true;
        }

        auto ExpandR8ToRgba8(const u8* src, u32 width, u32 height, TVector<u8>& outPixels) -> bool {
            if (src == nullptr || width == 0U || height == 0U) {
                return false;
            }

            const usize pixelCount = static_cast<usize>(width) * static_cast<usize>(height);
            outPixels.Resize(pixelCount * 4U);
            for (usize index = 0U; index < pixelCount; ++index) {
                const u8 value             = src[index];
                outPixels[index * 4U + 0U] = value;
                outPixels[index * 4U + 1U] = value;
                outPixels[index * 4U + 2U] = value;
                outPixels[index * 4U + 3U] = 255U;
            }
            return true;
        }

        auto ExpandRgb8ToRgba8(const u8* src, u32 width, u32 height, TVector<u8>& outPixels)
            -> bool {
            if (src == nullptr || width == 0U || height == 0U) {
                return false;
            }

            const usize pixelCount = static_cast<usize>(width) * static_cast<usize>(height);
            outPixels.Resize(pixelCount * 4U);
            for (usize index = 0U; index < pixelCount; ++index) {
                outPixels[index * 4U + 0U] = src[index * 3U + 0U];
                outPixels[index * 4U + 1U] = src[index * 3U + 1U];
                outPixels[index * 4U + 2U] = src[index * 3U + 2U];
                outPixels[index * 4U + 3U] = 255U;
            }
            return true;
        }
    } // namespace

    FTexture2DAsset::FTexture2DAsset(FTexture2DDesc desc, TVector<u8> pixels)
        : mDesc(desc), mPixels(Move(pixels)) {}

    auto DecodeTexture2DToRgba8(
        const FTexture2DAsset& asset, FTexture2DDesc& outDesc, TVector<u8>& outPixels) -> bool {
        const auto& srcDesc   = asset.GetDesc();
        const auto& srcPixels = asset.GetPixels();
        if (srcDesc.Width == 0U || srcDesc.Height == 0U || srcDesc.MipCount == 0U) {
            return false;
        }

        outDesc        = srcDesc;
        outDesc.Format = kTextureFormatRGBA8;
        outPixels.Clear();
        outPixels.Reserve(static_cast<usize>(srcDesc.Width) * static_cast<usize>(srcDesc.Height)
            * static_cast<usize>(srcDesc.MipCount) * 4U);

        const u32 srcBytesPerPixel = GetTextureBytesPerPixel(srcDesc.Format);
        u32       width            = srcDesc.Width;
        u32       height           = srcDesc.Height;
        usize     srcOffset        = 0U;

        for (u32 mip = 0U; mip < srcDesc.MipCount; ++mip) {
            const u32 srcSlicePitch =
                GetTextureMipSlicePitch(srcDesc.Format, width, height, srcBytesPerPixel);
            if (srcSlicePitch == 0U
                || srcOffset + static_cast<usize>(srcSlicePitch) > srcPixels.Size()) {
                return false;
            }

            TVector<u8> decodedMip{};
            const u8*   mipSrc  = srcPixels.Data() + srcOffset;
            bool        decoded = false;
            switch (srcDesc.Format) {
                case kTextureFormatR8:
                    decoded = ExpandR8ToRgba8(mipSrc, width, height, decodedMip);
                    break;
                case kTextureFormatRGB8:
                    decoded = ExpandRgb8ToRgba8(mipSrc, width, height, decodedMip);
                    break;
                case kTextureFormatRGBA8:
                    decodedMip.Resize(static_cast<usize>(srcSlicePitch));
                    for (usize byteIndex = 0U; byteIndex < static_cast<usize>(srcSlicePitch);
                        ++byteIndex) {
                        decodedMip[byteIndex] = mipSrc[byteIndex];
                    }
                    decoded = true;
                    break;
                case kTextureFormatBC1:
                    decoded = DecodeBc1Mip(mipSrc, width, height, decodedMip);
                    break;
                case kTextureFormatBC2:
                    decoded = DecodeBc2Mip(mipSrc, width, height, decodedMip);
                    break;
                case kTextureFormatBC3:
                    decoded = DecodeBc3Mip(mipSrc, width, height, decodedMip);
                    break;
                case kTextureFormatBC4:
                    decoded = DecodeBc4Mip(mipSrc, width, height, decodedMip);
                    break;
                case kTextureFormatBC5:
                    decoded = DecodeBc5Mip(mipSrc, width, height, decodedMip);
                    break;
                default:
                    decoded = false;
                    break;
            }

            if (!decoded) {
                outPixels.Clear();
                return false;
            }

            for (usize byteIndex = 0U; byteIndex < decodedMip.Size(); ++byteIndex) {
                outPixels.PushBack(decodedMip[byteIndex]);
            }
            srcOffset += static_cast<usize>(srcSlicePitch);
            width  = (width > 1U) ? (width >> 1U) : 1U;
            height = (height > 1U) ? (height >> 1U) : 1U;
        }

        return true;
    }
} // namespace AltinaEngine::Asset
