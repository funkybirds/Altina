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

    struct AE_ASSET_API FMeshBlobDesc {
        u32 VertexCount      = 0;
        u32 IndexCount       = 0;
        u32 VertexStride     = 0;
        u32 IndexType        = 0;
        u32 AttributeCount   = 0;
        u32 SubMeshCount     = 0;
        u32 AttributesOffset = 0;
        u32 SubMeshesOffset  = 0;
        u32 VertexDataOffset = 0;
        u32 IndexDataOffset  = 0;
        u32 VertexDataSize   = 0;
        u32 IndexDataSize    = 0;
        f32 BoundsMin[3]     = { 0.0f, 0.0f, 0.0f };
        f32 BoundsMax[3]     = { 0.0f, 0.0f, 0.0f };
        u32 Flags            = 0;
    };

    struct AE_ASSET_API FMeshVertexAttributeDesc {
        u32 Semantic         = 0;
        u32 SemanticIndex    = 0;
        u32 Format           = 0;
        u32 InputSlot        = 0;
        u32 AlignedOffset    = 0;
        u32 PerInstance      = 0;
        u32 InstanceStepRate = 0;
    };

    struct AE_ASSET_API FMeshSubMeshDesc {
        u32 IndexStart   = 0;
        u32 IndexCount   = 0;
        i32 BaseVertex   = 0;
        u32 MaterialSlot = 0;
    };

    struct AE_ASSET_API FAudioBlobDesc {
        u32 Codec            = 0;
        u32 SampleFormat     = 0;
        u32 Channels         = 0;
        u32 SampleRate       = 0;
        u32 FrameCount       = 0;
        u32 ChunkCount       = 0;
        u32 FramesPerChunk   = 0;
        u32 ChunkTableOffset = 0;
        u32 DataOffset       = 0;
        u32 DataSize         = 0;
    };

    struct AE_ASSET_API FAudioChunkDesc {
        u32 Offset = 0;
        u32 Size   = 0;
    };

    struct AE_ASSET_API FMaterialBlobDesc {
        u32 ShadingModel   = 0;
        u32 BlendMode      = 0;
        u32 Flags          = 0;
        f32 AlphaCutoff    = 0.0f;
        u32 ScalarCount    = 0;
        u32 VectorCount    = 0;
        u32 TextureCount   = 0;
        u32 ScalarsOffset  = 0;
        u32 VectorsOffset  = 0;
        u32 TexturesOffset = 0;
    };

    struct AE_ASSET_API FMaterialScalarParam {
        u32 NameHash = 0;
        f32 Value    = 0.0f;
    };

    struct AE_ASSET_API FMaterialVectorParam {
        u32 NameHash = 0;
        f32 Value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    };

    struct AE_ASSET_API FMaterialTextureParam {
        u32          NameHash = 0;
        FAssetHandle Texture;
        u32          SamplerFlags = 0;
    };

    struct AE_ASSET_API FModelBlobDesc {
        u32 NodeCount           = 0;
        u32 MeshRefCount        = 0;
        u32 MaterialSlotCount   = 0;
        u32 NodesOffset         = 0;
        u32 MeshRefsOffset      = 0;
        u32 MaterialSlotsOffset = 0;
    };

    struct AE_ASSET_API FModelNodeDesc {
        i32 ParentIndex    = -1;
        i32 MeshRefIndex   = -1;
        f32 Translation[3] = { 0.0f, 0.0f, 0.0f };
        f32 Rotation[4]    = { 0.0f, 0.0f, 0.0f, 1.0f };
        f32 Scale[3]       = { 1.0f, 1.0f, 1.0f };
    };

    struct AE_ASSET_API FModelMeshRef {
        FAssetHandle Mesh;
        u32          MaterialSlotOffset = 0;
        u32          MaterialSlotCount  = 0;
    };

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

    constexpr u32                kTextureFormatUnknown = 0;
    constexpr u32                kTextureFormatR8      = 1;
    constexpr u32                kTextureFormatRGB8    = 2;
    constexpr u32                kTextureFormatRGBA8   = 3;

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
