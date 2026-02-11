#pragma once

#include "Asset/AssetTypes.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Asset {
    constexpr u32 kAssetBlobMagic   = 0x31534141u; // "AAS1"
    constexpr u16 kAssetBlobVersion = 1;

    enum class EAssetBlobFlags : u8 {
        None = 0,
        SRGB = 1 << 0
    };

    [[nodiscard]] constexpr auto HasAssetBlobFlag(u8 flags, EAssetBlobFlags flag) noexcept -> bool {
        return (flags & static_cast<u8>(flag)) != 0;
    }

    [[nodiscard]] constexpr auto MakeAssetBlobFlags(bool srgb) noexcept -> u8 {
        return srgb ? static_cast<u8>(EAssetBlobFlags::SRGB) : 0U;
    }

    struct AE_ASSET_API FAssetBlobHeader {
        u32 Magic    = kAssetBlobMagic;
        u16 Version  = kAssetBlobVersion;
        u8  Type     = static_cast<u8>(EAssetType::Unknown);
        u8  Flags    = 0;
        u32 DescSize = 0;
        u32 DataSize = 0;
    };

    struct AE_ASSET_API FTexture2DBlobDesc {
        u32 Width    = 0;
        u32 Height   = 0;
        u32 Format   = 0;
        u32 MipCount = 0;
        u32 RowPitch = 0;
    };

    constexpr u32 kTextureFormatUnknown = 0;
    constexpr u32 kTextureFormatR8      = 1;
    constexpr u32 kTextureFormatRGB8    = 2;
    constexpr u32 kTextureFormatRGBA8   = 3;

    [[nodiscard]] constexpr auto GetTextureBytesPerPixel(u32 format) noexcept -> u32 {
        switch (format) {
            case kTextureFormatR8:
                return 1;
            case kTextureFormatRGB8:
                return 3;
            case kTextureFormatRGBA8:
                return 4;
            default:
                return 0;
        }
    }

} // namespace AltinaEngine::Asset
