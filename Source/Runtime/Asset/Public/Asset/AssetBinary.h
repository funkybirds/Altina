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
        u32 mMagic    = kAssetBlobMagic;
        u16 mVersion  = kAssetBlobVersion;
        u8  mType     = static_cast<u8>(EAssetType::Unknown);
        u8  mFlags    = 0;
        u32 mDescSize = 0;
        u32 mDataSize = 0;
    };

    struct AE_ASSET_API FTexture2DBlobDesc {
        u32 mWidth    = 0;
        u32 mHeight   = 0;
        u32 mFormat   = 0;
        u32 mMipCount = 0;
        u32 mRowPitch = 0;
    };

    struct AE_ASSET_API FCubeMapBlobDesc {
        u32 mSize     = 0;
        u32 mFormat   = 0;
        u32 mMipCount = 0;
        u32 mRowPitch = 0; // Row pitch for mip 0 (tightly packed).
    };

    struct AE_ASSET_API FMeshBlobDesc {
        u32 mVertexCount      = 0;
        u32 mIndexCount       = 0;
        u32 mVertexStride     = 0;
        u32 mIndexType        = 0;
        u32 mAttributeCount   = 0;
        u32 mSubMeshCount     = 0;
        u32 mAttributesOffset = 0;
        u32 mSubMeshesOffset  = 0;
        u32 mVertexDataOffset = 0;
        u32 mIndexDataOffset  = 0;
        u32 mVertexDataSize   = 0;
        u32 mIndexDataSize    = 0;
        f32 mBoundsMin[3]     = { 0.0f, 0.0f, 0.0f };
        f32 mBoundsMax[3]     = { 0.0f, 0.0f, 0.0f };
        u32 mFlags            = 0;
    };

    struct AE_ASSET_API FMeshVertexAttributeDesc {
        u32 mSemantic         = 0;
        u32 mSemanticIndex    = 0;
        u32 mFormat           = 0;
        u32 mInputSlot        = 0;
        u32 mAlignedOffset    = 0;
        u32 mPerInstance      = 0;
        u32 mInstanceStepRate = 0;
    };

    struct AE_ASSET_API FMeshSubMeshDesc {
        u32 mIndexStart   = 0;
        u32 mIndexCount   = 0;
        i32 mBaseVertex   = 0;
        u32 mMaterialSlot = 0;
    };

    struct AE_ASSET_API FAudioBlobDesc {
        u32 mCodec            = 0;
        u32 mSampleFormat     = 0;
        u32 mChannels         = 0;
        u32 mSampleRate       = 0;
        u32 mFrameCount       = 0;
        u32 mChunkCount       = 0;
        u32 mFramesPerChunk   = 0;
        u32 mChunkTableOffset = 0;
        u32 mDataOffset       = 0;
        u32 mDataSize         = 0;
    };

    struct AE_ASSET_API FAudioChunkDesc {
        u32 mOffset = 0;
        u32 mSize   = 0;
    };

    struct AE_ASSET_API FMaterialBlobDesc {
        u32 mShadingModel   = 0;
        u32 mBlendMode      = 0;
        u32 mFlags          = 0;
        f32 mAlphaCutoff    = 0.0f;
        u32 mScalarCount    = 0;
        u32 mVectorCount    = 0;
        u32 mTextureCount   = 0;
        u32 mScalarsOffset  = 0;
        u32 mVectorsOffset  = 0;
        u32 mTexturesOffset = 0;
    };

    struct AE_ASSET_API FMaterialScalarParam {
        u32 mNameHash = 0;
        f32 mValue    = 0.0f;
    };

    struct AE_ASSET_API FMaterialVectorParam {
        u32 mNameHash = 0;
        f32 mValue[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    };

    struct AE_ASSET_API FMaterialTextureParam {
        u32          mNameHash = 0;
        FAssetHandle mTexture;
        u32          mSamplerFlags = 0;
    };

    struct AE_ASSET_API FModelBlobDesc {
        u32 mNodeCount           = 0;
        u32 mMeshRefCount        = 0;
        u32 mMaterialSlotCount   = 0;
        u32 mNodesOffset         = 0;
        u32 mMeshRefsOffset      = 0;
        u32 mMaterialSlotsOffset = 0;
    };

    struct AE_ASSET_API FModelNodeDesc {
        i32 mParentIndex    = -1;
        i32 mMeshRefIndex   = -1;
        f32 mTranslation[3] = { 0.0f, 0.0f, 0.0f };
        f32 mRotation[4]    = { 0.0f, 0.0f, 0.0f, 1.0f };
        f32 mScale[3]       = { 1.0f, 1.0f, 1.0f };
    };

    struct AE_ASSET_API FModelMeshRef {
        FAssetHandle mMesh;
        u32          mMaterialSlotOffset = 0;
        u32          mMaterialSlotCount  = 0;
    };

    struct AE_ASSET_API FLevelBlobDesc {
        u32 mEncoding = 0;
    };

    constexpr u32                kLevelEncodingWorldBinary = 0;
    constexpr u32                kLevelEncodingWorldJson   = 1;

    constexpr u32                kMeshSemanticPosition = 0;
    constexpr u32                kMeshSemanticNormal   = 1;
    constexpr u32                kMeshSemanticTangent  = 2;
    constexpr u32                kMeshSemanticTexCoord = 3;
    constexpr u32                kMeshSemanticColor    = 4;

    constexpr u32                kMeshVertexMaskPosition  = 1u << 0;
    constexpr u32                kMeshVertexMaskNormal    = 1u << 1;
    constexpr u32                kMeshVertexMaskTexCoord0 = 1u << 2;

    constexpr u32                kMeshVertexFormatUnknown           = 0;
    constexpr u32                kMeshVertexFormatR32Float          = 1;
    constexpr u32                kMeshVertexFormatR32G32Float       = 2;
    constexpr u32                kMeshVertexFormatR32G32B32Float    = 3;
    constexpr u32                kMeshVertexFormatR32G32B32A32Float = 4;

    constexpr u32                kAudioCodecUnknown   = 0;
    constexpr u32                kAudioCodecPcm       = 1;
    constexpr u32                kAudioCodecOggVorbis = 2;

    constexpr u32                kAudioSampleFormatUnknown = 0;
    constexpr u32                kAudioSampleFormatPcm16   = 1;
    constexpr u32                kAudioSampleFormatPcm32f  = 2;

    [[nodiscard]] constexpr auto GetAudioBytesPerSample(u32 format) noexcept -> u32 {
        switch (format) {
            case kAudioSampleFormatPcm16:
                return 2;
            case kAudioSampleFormatPcm32f:
                return 4;
            default:
                return 0;
        }
    }

    constexpr u32                kTextureFormatUnknown  = 0;
    constexpr u32                kTextureFormatR8       = 1;
    constexpr u32                kTextureFormatRGB8     = 2;
    constexpr u32                kTextureFormatRGBA8    = 3;
    constexpr u32                kTextureFormatRGBA16F  = 4;
    constexpr u32                kTextureFormatBC1      = 5;
    constexpr u32                kTextureFormatBC2      = 6;
    constexpr u32                kTextureFormatBC3      = 7;
    constexpr u32                kTextureFormatBC4      = 8;
    constexpr u32                kTextureFormatBC5      = 9;
    constexpr u32                kTextureFormatBC6HUF16 = 10;
    constexpr u32                kTextureFormatBC6HSF16 = 11;
    constexpr u32                kTextureFormatBC7      = 12;

    [[nodiscard]] constexpr auto GetTextureBytesPerPixel(u32 format) noexcept -> u32 {
        switch (format) {
            case kTextureFormatR8:
                return 1;
            case kTextureFormatRGB8:
                return 3;
            case kTextureFormatRGBA8:
                return 4;
            case kTextureFormatRGBA16F:
                return 8;
            default:
                return 0;
        }
    }

    [[nodiscard]] constexpr auto IsTextureBlockCompressed(u32 format) noexcept -> bool {
        switch (format) {
            case kTextureFormatBC1:
            case kTextureFormatBC2:
            case kTextureFormatBC3:
            case kTextureFormatBC4:
            case kTextureFormatBC5:
            case kTextureFormatBC6HUF16:
            case kTextureFormatBC6HSF16:
            case kTextureFormatBC7:
                return true;
            default:
                return false;
        }
    }

    [[nodiscard]] constexpr auto GetTextureBlockBytes(u32 format) noexcept -> u32 {
        switch (format) {
            case kTextureFormatBC1:
            case kTextureFormatBC4:
                return 8;
            case kTextureFormatBC2:
            case kTextureFormatBC3:
            case kTextureFormatBC5:
            case kTextureFormatBC6HUF16:
            case kTextureFormatBC6HSF16:
            case kTextureFormatBC7:
                return 16;
            default:
                return 0;
        }
    }

    [[nodiscard]] constexpr auto GetTextureMipRowPitch(
        u32 format, u32 width, u32 bytesPerPixel = 0) noexcept -> u32 {
        if (width == 0U) {
            return 0U;
        }
        if (IsTextureBlockCompressed(format)) {
            const u32 blockBytes = GetTextureBlockBytes(format);
            const u32 blocksX    = (width + 3U) / 4U;
            return blocksX * blockBytes;
        }
        return width * bytesPerPixel;
    }

    [[nodiscard]] constexpr auto GetTextureMipSlicePitch(
        u32 format, u32 width, u32 height, u32 bytesPerPixel = 0) noexcept -> u32 {
        if (width == 0U || height == 0U) {
            return 0U;
        }
        if (IsTextureBlockCompressed(format)) {
            const u32 rowPitch = GetTextureMipRowPitch(format, width, bytesPerPixel);
            const u32 blocksY  = (height + 3U) / 4U;
            return rowPitch * blocksY;
        }
        return GetTextureMipRowPitch(format, width, bytesPerPixel) * height;
    }

    constexpr u32                kMeshIndexTypeUint16 = 0;
    constexpr u32                kMeshIndexTypeUint32 = 1;

    [[nodiscard]] constexpr auto GetMeshIndexStride(u32 indexType) noexcept -> u32 {
        switch (indexType) {
            case kMeshIndexTypeUint16:
                return 2;
            case kMeshIndexTypeUint32:
                return 4;
            default:
                return 0;
        }
    }

} // namespace AltinaEngine::Asset
