#include <filesystem>
#include <system_error>
#include <cstring>
#include <cmath>
#include <fstream>

#include "Asset/AssetBinary.h"
#include "Asset/AssetBundle.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Asset/AudioAsset.h"
#include "Asset/AudioLoader.h"
#include "Asset/MaterialAsset.h"
#include "Asset/MaterialLoader.h"
#include "Asset/MeshAsset.h"
#include "Asset/MeshLoader.h"
#include "Asset/LevelAsset.h"
#include "Asset/LevelLoader.h"
#include "Asset/Texture2DAsset.h"
#include "Asset/Texture2DLoader.h"
#include "TestHarness.h"

namespace {
    namespace Container = AltinaEngine::Core::Container;
    using AltinaEngine::u16;
    using AltinaEngine::u32;
    using AltinaEngine::u64;
    using AltinaEngine::u8;
    using AltinaEngine::usize;
    using AltinaEngine::Asset::FAssetBundleReader;
    using AltinaEngine::Asset::FAssetDesc;
    using AltinaEngine::Asset::FAssetHandle;
    using AltinaEngine::Asset::FAssetManager;
    using AltinaEngine::Asset::FAssetRegistry;
    using AltinaEngine::Asset::FAudioAsset;
    using AltinaEngine::Asset::FAudioLoader;
    using AltinaEngine::Asset::FLevelAsset;
    using AltinaEngine::Asset::FLevelLoader;
    using AltinaEngine::Asset::FMaterialAsset;
    using AltinaEngine::Asset::FMaterialLoader;
    using AltinaEngine::Asset::FMeshAsset;
    using AltinaEngine::Asset::FMeshLoader;
    using AltinaEngine::Asset::FTexture2DAsset;
    using AltinaEngine::Asset::FTexture2DDesc;
    using AltinaEngine::Asset::FTexture2DLoader;
    using AltinaEngine::Asset::GetAudioBytesPerSample;
    using AltinaEngine::Asset::GetMeshIndexStride;
    using AltinaEngine::Asset::GetTextureBytesPerPixel;
    using Container::FNativeStringView;
    using Container::FString;
    using Container::TVector;

    auto ToFString(const std::filesystem::path& path) -> FString {
        FString out;
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        const auto wide = path.wstring();
        if (!wide.empty()) {
            out.Append(wide.c_str(), wide.size());
        }
#else
        const auto narrow = path.string();
        if (!narrow.empty()) {
            out.Append(narrow.c_str(), narrow.size());
        }
#endif
        return out;
    }

    auto GetRegistryPath() -> std::filesystem::path {
#if defined(AE_SOURCE_DIR)
        return std::filesystem::path(AE_SOURCE_DIR) / "build" / "Cooked" / "Win64" / "Registry"
            / "AssetRegistry.json";
#else
        return std::filesystem::current_path() / "build" / "Cooked" / "Win64" / "Registry"
            / "AssetRegistry.json";
#endif
    }

    struct FScopedCurrentPath {
        std::filesystem::path Prev;
        bool                  Ok = false;

        explicit FScopedCurrentPath(const std::filesystem::path& path) {
            std::error_code ec;
            Prev = std::filesystem::current_path(ec);
            if (ec) {
                return;
            }
            std::filesystem::current_path(path, ec);
            Ok = !ec;
        }

        ~FScopedCurrentPath() {
            if (!Ok) {
                return;
            }
            std::error_code ec;
            std::filesystem::current_path(Prev, ec);
        }
    };

    class FTestAssetStream final : public AltinaEngine::Asset::IAssetStream {
    public:
        explicit FTestAssetStream(const TVector<u8>& data)
            : mData(data.Data()), mSize(data.Size()) {}

        [[nodiscard]] auto Size() const noexcept -> usize override { return mSize; }
        [[nodiscard]] auto Tell() const noexcept -> usize override { return mOffset; }

        void Seek(usize offset) noexcept override { mOffset = (offset > mSize) ? mSize : offset; }

        auto Read(void* outBuffer, usize bytesToRead) -> usize override {
            if (outBuffer == nullptr || bytesToRead == 0U || mData == nullptr) {
                return 0U;
            }

            const usize remaining = mSize - mOffset;
            const usize toRead    = (bytesToRead > remaining) ? remaining : bytesToRead;
            if (toRead == 0U) {
                return 0U;
            }

            std::memcpy(outBuffer, mData + mOffset, static_cast<size_t>(toRead));
            mOffset += toRead;
            return toRead;
        }

    private:
        const u8* mData   = nullptr;
        usize     mSize   = 0;
        usize     mOffset = 0;
    };

    auto ComputePackedMipSize(const FTexture2DDesc& desc) -> u64 {
        const u32 bytesPerPixel = GetTextureBytesPerPixel(desc.Format);
        if ((bytesPerPixel == 0U && !AltinaEngine::Asset::IsTextureBlockCompressed(desc.Format))
            || desc.Width == 0U || desc.Height == 0U || desc.MipCount == 0U) {
            return 0U;
        }

        u32 width  = desc.Width;
        u32 height = desc.Height;
        u64 total  = 0U;
        for (u32 mip = 0; mip < desc.MipCount; ++mip) {
            total += AltinaEngine::Asset::GetTextureMipSlicePitch(
                desc.Format, width, height, bytesPerPixel);
            width  = (width > 1U) ? (width >> 1U) : 1U;
            height = (height > 1U) ? (height >> 1U) : 1U;
        }
        return total;
    }
} // namespace

TEST_CASE("Asset.Texture2D.EngineFormat.Load") {
    FAssetRegistry registry;
    const auto     registryPath = GetRegistryPath();
    REQUIRE(registry.LoadFromJsonFile(ToFString(registryPath)));

    const auto         cookedRoot = registryPath.parent_path().parent_path();
    FScopedCurrentPath scopedPath(cookedRoot);
    REQUIRE(scopedPath.Ok);

    FAssetManager    manager;
    FTexture2DLoader loader;
    manager.SetRegistry(&registry);
    manager.RegisterLoader(&loader);

    const FAssetHandle handle = registry.FindByPath(TEXT("demo/minimal/checker"));
    REQUIRE(handle.IsValid());

    const FAssetDesc* registryDesc = registry.GetDesc(handle);
    REQUIRE(registryDesc != nullptr);

    auto asset = manager.Load(handle);
    REQUIRE(asset);

    auto* texture = static_cast<FTexture2DAsset*>(asset.Get());
    REQUIRE(texture != nullptr);

    const auto& desc = texture->GetDesc();
    REQUIRE_EQ(desc.Width, registryDesc->mTexture.Width);
    REQUIRE_EQ(desc.Height, registryDesc->mTexture.Height);
    REQUIRE_EQ(desc.MipCount, registryDesc->mTexture.MipCount);
    REQUIRE_EQ(desc.Format, registryDesc->mTexture.Format);
    REQUIRE_EQ(desc.SRGB, registryDesc->mTexture.SRGB);

    const u64 expectedSize = ComputePackedMipSize(desc);
    REQUIRE(expectedSize > 0U);
    REQUIRE_EQ(texture->GetPixels().Size(), static_cast<usize>(expectedSize));

    manager.UnregisterLoader(&loader);
    manager.SetRegistry(nullptr);
}

TEST_CASE("Asset.Mesh.EngineFormat.Load") {
    FAssetRegistry registry;
    const auto     registryPath = GetRegistryPath();
    REQUIRE(registry.LoadFromJsonFile(ToFString(registryPath)));

    const auto         cookedRoot = registryPath.parent_path().parent_path();
    FScopedCurrentPath scopedPath(cookedRoot);
    REQUIRE(scopedPath.Ok);

    FAssetManager manager;
    FMeshLoader   loader;
    manager.SetRegistry(&registry);
    manager.RegisterLoader(&loader);

    const FAssetHandle handle = registry.FindByPath(TEXT("demo/minimal/triangle"));
    REQUIRE(handle.IsValid());

    const FAssetDesc* registryDesc = registry.GetDesc(handle);
    REQUIRE(registryDesc != nullptr);

    auto asset = manager.Load(handle);
    REQUIRE(asset);

    auto* mesh = static_cast<FMeshAsset*>(asset.Get());
    REQUIRE(mesh != nullptr);

    const auto& desc = mesh->GetDesc();
    REQUIRE(desc.mVertexCount > 0U);
    REQUIRE(desc.mIndexCount > 0U);
    REQUIRE(desc.mVertexStride > 0U);

    const u32 indexStride = GetMeshIndexStride(desc.mIndexType);
    REQUIRE(indexStride > 0U);

    REQUIRE_EQ(mesh->GetVertexData().Size(),
        static_cast<usize>(desc.mVertexCount) * static_cast<usize>(desc.mVertexStride));
    REQUIRE_EQ(mesh->GetIndexData().Size(),
        static_cast<usize>(desc.mIndexCount) * static_cast<usize>(indexStride));

    REQUIRE(!mesh->GetSubMeshes().IsEmpty());
    if (!mesh->GetSubMeshes().IsEmpty()) {
        const auto& subMesh = mesh->GetSubMeshes().Front();
        REQUIRE_EQ(subMesh.mIndexCount, desc.mIndexCount);
    }

    manager.UnregisterLoader(&loader);
    manager.SetRegistry(nullptr);
}

TEST_CASE("Asset.Texture2D.BlockCompressed.Load") {
    AltinaEngine::Asset::FTexture2DBlobDesc blobDesc{};
    blobDesc.mWidth    = 8U;
    blobDesc.mHeight   = 8U;
    blobDesc.mFormat   = AltinaEngine::Asset::kTextureFormatBC1;
    blobDesc.mMipCount = 2U;
    blobDesc.mRowPitch =
        AltinaEngine::Asset::GetTextureMipRowPitch(blobDesc.mFormat, blobDesc.mWidth, 0U);

    const u32 payloadSize =
        AltinaEngine::Asset::GetTextureMipSlicePitch(blobDesc.mFormat, 8U, 8U, 0U)
        + AltinaEngine::Asset::GetTextureMipSlicePitch(blobDesc.mFormat, 4U, 4U, 0U);

    AltinaEngine::Asset::FAssetBlobHeader header{};
    header.mType     = static_cast<u8>(AltinaEngine::Asset::EAssetType::Texture2D);
    header.mDescSize = static_cast<u32>(sizeof(blobDesc));
    header.mDataSize = payloadSize;

    TVector<u8> cooked{};
    cooked.Resize(sizeof(header) + sizeof(blobDesc) + payloadSize);
    std::memcpy(cooked.Data(), &header, sizeof(header));
    std::memcpy(cooked.Data() + sizeof(header), &blobDesc, sizeof(blobDesc));
    for (u32 index = 0U; index < payloadSize; ++index) {
        cooked[sizeof(header) + sizeof(blobDesc) + index] = static_cast<u8>(index + 1U);
    }

    FTestAssetStream stream(cooked);
    FTexture2DLoader loader{};

    FAssetDesc       desc{};
    desc.mTexture.Width    = blobDesc.mWidth;
    desc.mTexture.Height   = blobDesc.mHeight;
    desc.mTexture.MipCount = blobDesc.mMipCount;
    desc.mTexture.Format   = blobDesc.mFormat;
    desc.mTexture.SRGB     = false;

    auto asset = loader.Load(desc, stream);
    REQUIRE(asset);

    auto* texture = static_cast<FTexture2DAsset*>(asset.Get());
    REQUIRE(texture != nullptr);
    REQUIRE_EQ(texture->GetDesc().Format, AltinaEngine::Asset::kTextureFormatBC1);
    REQUIRE_EQ(texture->GetDesc().MipCount, 2U);
    REQUIRE_EQ(texture->GetPixels().Size(), static_cast<usize>(payloadSize));
    REQUIRE(std::memcmp(texture->GetPixels().Data(),
                cooked.Data() + sizeof(header) + sizeof(blobDesc), payloadSize)
        == 0);
}

TEST_CASE("Asset.MaterialTemplate.Json.Load") {
    constexpr const char* kShaderUuid = "11111111-2222-3333-4444-555555555555";
    AltinaEngine::FUuid   shaderUuid;
    REQUIRE(AltinaEngine::FUuid::TryParse(FNativeStringView(kShaderUuid), shaderUuid));

    AltinaEngine::Core::Container::FNativeString json;
    json.Append("{\"name\":\"TestMaterial\",\"passes\":{\"BasePass\":{\"shaders\":{");
    json.Append("\"vs\":{\"uuid\":\"");
    json.Append(kShaderUuid);
    json.Append("\",\"type\":\"shader\",\"entry\":\"VSMain\"},");
    json.Append("\"ps\":{\"uuid\":\"");
    json.Append(kShaderUuid);
    json.Append("\",\"type\":\"shader\",\"entry\":\"PSMain\"}");
    json.Append("},\"raster_overrides\":{");
    json.Append("\"cull\":\"none\",");
    json.Append("\"front_face\":\"cw\",");
    json.Append("\"depth_bias\":2,");
    json.Append("\"depth_clip\":false");
    json.Append("}}},\"precompile_variants\":[[\"PARAM_A\",\"PARAM_B\"]]}");

    TVector<u8> cooked;
    cooked.Resize(json.Length());
    std::memcpy(cooked.Data(), json.GetData(), static_cast<size_t>(json.Length()));

    FTestAssetStream stream(cooked);
    FMaterialLoader  loader;

    FAssetDesc       desc{};
    auto             asset = loader.Load(desc, stream);
    REQUIRE(asset);

    auto* material = static_cast<FMaterialAsset*>(asset.Get());
    REQUIRE(material != nullptr);
    REQUIRE_EQ(material->GetName(), FString(TEXT("TestMaterial")));

    const auto& passes = material->GetPasses();
    REQUIRE_EQ(passes.Size(), static_cast<usize>(1));
    const auto& pass = passes[0];
    REQUIRE_EQ(pass.mName, FString(TEXT("BasePass")));
    REQUIRE(pass.mHasVertex);
    REQUIRE(pass.mHasPixel);
    REQUIRE_EQ(pass.mVertex.mAsset.mUuid, shaderUuid);
    REQUIRE_EQ(pass.mVertex.mAsset.mType, AltinaEngine::Asset::EAssetType::Shader);
    REQUIRE_EQ(pass.mVertex.mEntry, FString(TEXT("VSMain")));
    REQUIRE_EQ(pass.mPixel.mAsset.mUuid, shaderUuid);
    REQUIRE_EQ(pass.mPixel.mAsset.mType, AltinaEngine::Asset::EAssetType::Shader);
    REQUIRE_EQ(pass.mPixel.mEntry, FString(TEXT("PSMain")));

    REQUIRE(pass.mRasterOverrides.HasAny());
    REQUIRE(pass.mRasterOverrides.mHasCullMode);
    REQUIRE_EQ(pass.mRasterOverrides.mCullMode, AltinaEngine::Asset::EMaterialRasterCullMode::None);
    REQUIRE(pass.mRasterOverrides.mHasFrontFace);
    REQUIRE_EQ(pass.mRasterOverrides.mFrontFace, AltinaEngine::Asset::EMaterialRasterFrontFace::CW);
    REQUIRE(pass.mRasterOverrides.mHasDepthBias);
    REQUIRE_EQ(pass.mRasterOverrides.mDepthBias, 2);
    REQUIRE(pass.mRasterOverrides.mHasDepthClip);
    REQUIRE(!pass.mRasterOverrides.mDepthClip);

    const auto& variants = material->GetPrecompileVariants();
    REQUIRE_EQ(variants.Size(), static_cast<usize>(1));
    REQUIRE_EQ(variants[0].Size(), static_cast<usize>(2));
    REQUIRE_EQ(variants[0][0], FString(TEXT("PARAM_A")));
    REQUIRE_EQ(variants[0][1], FString(TEXT("PARAM_B")));
}

TEST_CASE("Asset.Bundle.RoundTrip") {
    AltinaEngine::FUuid::FBytes uuidBytes{};
    uuidBytes[0]  = 0xAA;
    uuidBytes[1]  = 0xBB;
    uuidBytes[2]  = 0xCC;
    uuidBytes[3]  = 0xDD;
    uuidBytes[15] = 0xEE;
    AltinaEngine::FUuid assetUuid(uuidBytes);

    TVector<u8>         payload;
    payload.Resize(12);
    for (usize i = 0; i < payload.Size(); ++i) {
        payload[i] = static_cast<u8>(i + 1U);
    }

    const auto bundlePath = std::filesystem::current_path() / "BundleRoundTrip.pak";
    {
        std::ofstream file(bundlePath, std::ios::binary);
        REQUIRE(file.good());

        AltinaEngine::Asset::FBundleHeader header{};
        header.mMagic   = AltinaEngine::Asset::kBundleMagic;
        header.mVersion = AltinaEngine::Asset::kBundleVersion;

        file.write(
            reinterpret_cast<const char*>(&header), static_cast<std::streamsize>(sizeof(header)));

        AltinaEngine::Asset::FBundleIndexEntry entry{};
        const auto&                            bytes = assetUuid.GetBytes();
        for (usize index = 0; index < AltinaEngine::FUuid::kByteCount; ++index) {
            entry.mUuid[index] = bytes[index];
        }
        entry.mType             = static_cast<u32>(AltinaEngine::Asset::EAssetType::Texture2D);
        entry.mCompression      = static_cast<u32>(AltinaEngine::Asset::EBundleCompression::None);
        entry.mOffset           = sizeof(header);
        entry.mSize             = static_cast<u64>(payload.Size());
        entry.mRawSize          = static_cast<u64>(payload.Size());
        entry.mChunkCount       = 0;
        entry.mChunkTableOffset = 0;

        file.write(reinterpret_cast<const char*>(payload.Data()),
            static_cast<std::streamsize>(payload.Size()));

        const u64                               indexOffset = entry.mOffset + entry.mSize;
        AltinaEngine::Asset::FBundleIndexHeader indexHeader{};
        indexHeader.mEntryCount      = 1;
        indexHeader.mStringTableSize = 0;

        file.write(reinterpret_cast<const char*>(&indexHeader),
            static_cast<std::streamsize>(sizeof(indexHeader)));
        file.write(
            reinterpret_cast<const char*>(&entry), static_cast<std::streamsize>(sizeof(entry)));

        header.mIndexOffset = indexOffset;
        header.mIndexSize   = sizeof(indexHeader) + sizeof(entry);
        header.mBundleSize  = header.mIndexOffset + header.mIndexSize;

        file.seekp(0, std::ios::beg);
        file.write(
            reinterpret_cast<const char*>(&header), static_cast<std::streamsize>(sizeof(header)));
    }

    FAssetBundleReader reader;
    REQUIRE(reader.Open(ToFString(bundlePath)));

    AltinaEngine::Asset::FBundleIndexEntry readEntry{};
    REQUIRE(reader.GetEntry(assetUuid, readEntry));
    REQUIRE_EQ(readEntry.mSize, static_cast<u64>(payload.Size()));

    TVector<u8> outBytes;
    REQUIRE(reader.ReadEntry(readEntry, outBytes));
    REQUIRE_EQ(outBytes.Size(), payload.Size());
    REQUIRE(std::memcmp(outBytes.Data(), payload.Data(), static_cast<size_t>(payload.Size())) == 0);

    reader.Close();
    std::error_code ec;
    std::filesystem::remove(bundlePath, ec);
}

TEST_CASE("Asset.Audio.EngineFormat.Load") {
    const u32 channels       = 1;
    const u32 sampleRate     = 48000;
    const u32 frameCount     = 8;
    const u32 framesPerChunk = 4;
    const u32 chunkCount     = 2;
    const u32 sampleFormat   = AltinaEngine::Asset::kAudioSampleFormatPcm16;

    const u32 bytesPerSample = GetAudioBytesPerSample(sampleFormat);
    REQUIRE(bytesPerSample == 2U);

    const u32 dataSize        = frameCount * channels * bytesPerSample;
    const u32 chunkTableBytes = chunkCount * sizeof(AltinaEngine::Asset::FAudioChunkDesc);

    AltinaEngine::Asset::FAudioBlobDesc blobDesc{};
    blobDesc.mCodec            = AltinaEngine::Asset::kAudioCodecPcm;
    blobDesc.mSampleFormat     = sampleFormat;
    blobDesc.mChannels         = channels;
    blobDesc.mSampleRate       = sampleRate;
    blobDesc.mFrameCount       = frameCount;
    blobDesc.mChunkCount       = chunkCount;
    blobDesc.mFramesPerChunk   = framesPerChunk;
    blobDesc.mChunkTableOffset = 0;
    blobDesc.mDataOffset       = chunkTableBytes;
    blobDesc.mDataSize         = dataSize;

    AltinaEngine::Asset::FAssetBlobHeader header{};
    header.mType     = static_cast<u8>(AltinaEngine::Asset::EAssetType::Audio);
    header.mDescSize = static_cast<u32>(sizeof(AltinaEngine::Asset::FAudioBlobDesc));
    header.mDataSize = blobDesc.mDataOffset + blobDesc.mDataSize;

    TVector<u8> cooked;
    cooked.Resize(sizeof(header) + sizeof(blobDesc) + header.mDataSize);

    u8* writePtr = cooked.Data();
    std::memcpy(writePtr, &header, sizeof(header));
    writePtr += sizeof(header);
    std::memcpy(writePtr, &blobDesc, sizeof(blobDesc));
    writePtr += sizeof(blobDesc);

    AltinaEngine::Asset::FAudioChunkDesc chunks[2]{};
    chunks[0].mOffset = blobDesc.mDataOffset;
    chunks[0].mSize   = dataSize / 2;
    chunks[1].mOffset = blobDesc.mDataOffset + chunks[0].mSize;
    chunks[1].mSize   = dataSize - chunks[0].mSize;
    std::memcpy(writePtr + blobDesc.mChunkTableOffset, chunks, sizeof(chunks));

    const u16 samples[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    std::memcpy(writePtr + blobDesc.mDataOffset, samples, sizeof(samples));

    FTestAssetStream stream(cooked);
    FAudioLoader     loader;

    FAssetDesc       desc{};
    desc.mAudio.Codec      = AltinaEngine::Asset::kAudioCodecPcm;
    desc.mAudio.Channels   = channels;
    desc.mAudio.SampleRate = sampleRate;

    auto asset = loader.Load(desc, stream);
    REQUIRE(asset);

    auto* audio = static_cast<FAudioAsset*>(asset.Get());
    REQUIRE(audio != nullptr);

    const auto& runtime = audio->GetDesc();
    REQUIRE_EQ(runtime.mCodec, blobDesc.mCodec);
    REQUIRE_EQ(runtime.mSampleFormat, blobDesc.mSampleFormat);
    REQUIRE_EQ(runtime.mChannels, blobDesc.mChannels);
    REQUIRE_EQ(runtime.mSampleRate, blobDesc.mSampleRate);
    REQUIRE_EQ(runtime.mFrameCount, blobDesc.mFrameCount);
    REQUIRE_EQ(runtime.mFramesPerChunk, blobDesc.mFramesPerChunk);

    REQUIRE_EQ(audio->GetChunks().Size(), static_cast<usize>(chunkCount));
    REQUIRE_EQ(audio->GetData().Size(), static_cast<usize>(dataSize));
}

TEST_CASE("Asset.Audio.EngineFormat.LoadFromRegistry") {
    FAssetRegistry registry;
    const auto     registryPath = GetRegistryPath();
    REQUIRE(registry.LoadFromJsonFile(ToFString(registryPath)));

    const auto         cookedRoot = registryPath.parent_path().parent_path();
    FScopedCurrentPath scopedPath(cookedRoot);
    REQUIRE(scopedPath.Ok);

    FAssetManager manager;
    FAudioLoader  loader;
    manager.SetRegistry(&registry);
    manager.RegisterLoader(&loader);

    const FAssetHandle handle = registry.FindByPath(TEXT("demo/minimal/beep"));
    REQUIRE(handle.IsValid());
    if (!handle.IsValid()) {
        manager.UnregisterLoader(&loader);
        manager.SetRegistry(nullptr);
        return;
    }

    const FAssetDesc* registryDesc = registry.GetDesc(handle);
    REQUIRE(registryDesc != nullptr);
    if (registryDesc == nullptr) {
        manager.UnregisterLoader(&loader);
        manager.SetRegistry(nullptr);
        return;
    }

    auto asset = manager.Load(handle);
    REQUIRE(asset);
    if (!asset) {
        manager.UnregisterLoader(&loader);
        manager.SetRegistry(nullptr);
        return;
    }

    auto* audio = static_cast<FAudioAsset*>(asset.Get());
    REQUIRE(audio != nullptr);
    if (audio == nullptr) {
        manager.UnregisterLoader(&loader);
        manager.SetRegistry(nullptr);
        return;
    }

    const auto& runtime = audio->GetDesc();
    REQUIRE_EQ(runtime.mCodec, registryDesc->mAudio.Codec);
    REQUIRE_EQ(runtime.mChannels, registryDesc->mAudio.Channels);
    REQUIRE_EQ(runtime.mSampleRate, registryDesc->mAudio.SampleRate);
    REQUIRE(runtime.mFrameCount > 0U);
    REQUIRE(!audio->GetData().IsEmpty());

    if (registryDesc->mAudio.DurationSeconds > 0.0f) {
        const float duration =
            static_cast<float>(runtime.mFrameCount) / static_cast<float>(runtime.mSampleRate);
        REQUIRE(std::abs(duration - registryDesc->mAudio.DurationSeconds) < 0.02f);
    }

    manager.UnregisterLoader(&loader);
    manager.SetRegistry(nullptr);
}

TEST_CASE("Asset.Level.EngineFormat.Load") {
    const char* levelJson = "{\"version\":2,\"worldId\":1,\"objects\":[]}";
    const usize jsonSize  = std::strlen(levelJson);

    AltinaEngine::Asset::FAssetBlobHeader header{};
    header.mType     = static_cast<u8>(AltinaEngine::Asset::EAssetType::Level);
    header.mDescSize = static_cast<u32>(sizeof(AltinaEngine::Asset::FLevelBlobDesc));
    header.mDataSize = static_cast<u32>(jsonSize);

    AltinaEngine::Asset::FLevelBlobDesc levelDesc{};
    levelDesc.mEncoding = AltinaEngine::Asset::kLevelEncodingWorldJson;

    TVector<u8> cooked{};
    cooked.Resize(sizeof(header) + sizeof(levelDesc) + jsonSize);
    std::memcpy(cooked.Data(), &header, sizeof(header));
    std::memcpy(cooked.Data() + sizeof(header), &levelDesc, sizeof(levelDesc));
    std::memcpy(cooked.Data() + sizeof(header) + sizeof(levelDesc), levelJson, jsonSize);

    FTestAssetStream stream(cooked);
    FLevelLoader     loader{};

    FAssetDesc       desc{};
    desc.mLevel.Encoding = AltinaEngine::Asset::kLevelEncodingWorldJson;
    desc.mLevel.ByteSize = static_cast<u32>(jsonSize);

    auto asset = loader.Load(desc, stream);
    REQUIRE(asset);

    const auto* level = static_cast<FLevelAsset*>(asset.Get());
    REQUIRE(level != nullptr);
    REQUIRE_EQ(level->GetEncoding(), AltinaEngine::Asset::kLevelEncodingWorldJson);
    REQUIRE_EQ(level->GetPayload().Size(), jsonSize);
}

TEST_CASE("Asset.Registry.LevelType.Parse") {
    AltinaEngine::Asset::FAssetRegistry          registry{};
    AltinaEngine::Core::Container::FNativeString text{};
    text.Append(
        "{\"SchemaVersion\":1,\"Assets\":[{\"Uuid\":\"12345678-1234-1234-1234-1234567890ab\","
        "\"Type\":\"Level\",\"VirtualPath\":\"demo/minimal/levels/default\",\"CookedPath\":\"Assets/"
        "12345678-1234-1234-1234-1234567890ab.bin\",\"Dependencies\":[],\"Desc\":{\"Encoding\":1,"
        "\"ByteSize\":32}}],\"Redirectors\":[]}");

    REQUIRE(registry.LoadFromJsonText(text.ToView()));
    const auto handle = registry.FindByPath(TEXT("demo/minimal/levels/default"));
    REQUIRE(handle.IsValid());
    REQUIRE_EQ(handle.mType, AltinaEngine::Asset::EAssetType::Level);

    const auto* desc = registry.GetDesc(handle);
    REQUIRE(desc != nullptr);
    REQUIRE_EQ(desc->mLevel.Encoding, 1U);
    REQUIRE_EQ(desc->mLevel.ByteSize, 32U);
}
