#include <filesystem>
#include <system_error>
#include <cstring>
#include <cmath>

#include "Asset/AssetBinary.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Asset/AudioAsset.h"
#include "Asset/AudioLoader.h"
#include "Asset/MeshAsset.h"
#include "Asset/MeshLoader.h"
#include "Asset/Texture2DAsset.h"
#include "Asset/Texture2DLoader.h"
#include "TestHarness.h"

namespace {
    using AltinaEngine::u8;
    using AltinaEngine::u16;
    using AltinaEngine::u32;
    using AltinaEngine::u64;
    using AltinaEngine::usize;
    using AltinaEngine::Asset::FAssetDesc;
    using AltinaEngine::Asset::FAssetHandle;
    using AltinaEngine::Asset::FAssetManager;
    using AltinaEngine::Asset::FAssetRegistry;
    using AltinaEngine::Asset::FAudioAsset;
    using AltinaEngine::Asset::FAudioLoader;
    using AltinaEngine::Asset::FMeshAsset;
    using AltinaEngine::Asset::FMeshLoader;
    using AltinaEngine::Asset::FTexture2DAsset;
    using AltinaEngine::Asset::FTexture2DDesc;
    using AltinaEngine::Asset::FTexture2DLoader;
    using AltinaEngine::Asset::GetAudioBytesPerSample;
    using AltinaEngine::Asset::GetMeshIndexStride;
    using AltinaEngine::Asset::GetTextureBytesPerPixel;
    using AltinaEngine::Core::Container::FString;
    using AltinaEngine::Core::Container::TVector;

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
        return std::filesystem::path(AE_SOURCE_DIR)
            / "build" / "Cooked" / "Win64" / "Registry" / "AssetRegistry.json";
    #else
        return std::filesystem::current_path()
            / "build" / "Cooked" / "Win64" / "Registry" / "AssetRegistry.json";
#endif
    }

    struct FScopedCurrentPath {
        std::filesystem::path Prev;
        bool Ok = false;

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
            : mData(data.Data())
            , mSize(data.Size()) {}

        [[nodiscard]] auto Size() const noexcept -> usize override { return mSize; }
        [[nodiscard]] auto Tell() const noexcept -> usize override { return mOffset; }

        void Seek(usize offset) noexcept override {
            mOffset = (offset > mSize) ? mSize : offset;
        }

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
        if (bytesPerPixel == 0U || desc.Width == 0U || desc.Height == 0U || desc.MipCount == 0U) {
            return 0U;
        }

        u32 width = desc.Width;
        u32 height = desc.Height;
        u64 total = 0U;
        for (u32 mip = 0; mip < desc.MipCount; ++mip) {
            total += static_cast<u64>(width) * static_cast<u64>(height) * bytesPerPixel;
            width = (width > 1U) ? (width >> 1U) : 1U;
            height = (height > 1U) ? (height >> 1U) : 1U;
        }
        return total;
    }
} // namespace

TEST_CASE("Asset.Texture2D.EngineFormat.Load") {
    FAssetRegistry registry;
    const auto registryPath = GetRegistryPath();
    REQUIRE(registry.LoadFromJsonFile(ToFString(registryPath)));

    const auto cookedRoot = registryPath.parent_path().parent_path();
    FScopedCurrentPath scopedPath(cookedRoot);
    REQUIRE(scopedPath.Ok);

    FAssetManager manager;
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
    REQUIRE_EQ(desc.Width, registryDesc->Texture.Width);
    REQUIRE_EQ(desc.Height, registryDesc->Texture.Height);
    REQUIRE_EQ(desc.MipCount, registryDesc->Texture.MipCount);
    REQUIRE_EQ(desc.Format, registryDesc->Texture.Format);
    REQUIRE_EQ(desc.SRGB, registryDesc->Texture.SRGB);

    const u64 expectedSize = ComputePackedMipSize(desc);
    REQUIRE(expectedSize > 0U);
    REQUIRE_EQ(texture->GetPixels().Size(), static_cast<usize>(expectedSize));

    manager.UnregisterLoader(&loader);
    manager.SetRegistry(nullptr);
}

TEST_CASE("Asset.Mesh.EngineFormat.Load") {
    FAssetRegistry registry;
    const auto registryPath = GetRegistryPath();
    REQUIRE(registry.LoadFromJsonFile(ToFString(registryPath)));

    const auto cookedRoot = registryPath.parent_path().parent_path();
    FScopedCurrentPath scopedPath(cookedRoot);
    REQUIRE(scopedPath.Ok);

    FAssetManager manager;
    FMeshLoader loader;
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
    REQUIRE(desc.VertexCount > 0U);
    REQUIRE(desc.IndexCount > 0U);
    REQUIRE(desc.VertexStride > 0U);

    const u32 indexStride = GetMeshIndexStride(desc.IndexType);
    REQUIRE(indexStride > 0U);

    REQUIRE_EQ(mesh->GetVertexData().Size(),
        static_cast<usize>(desc.VertexCount) * static_cast<usize>(desc.VertexStride));
    REQUIRE_EQ(mesh->GetIndexData().Size(),
        static_cast<usize>(desc.IndexCount) * static_cast<usize>(indexStride));

    REQUIRE(!mesh->GetSubMeshes().IsEmpty());
    if (!mesh->GetSubMeshes().IsEmpty()) {
        const auto& subMesh = mesh->GetSubMeshes().Front();
        REQUIRE_EQ(subMesh.IndexCount, desc.IndexCount);
    }

    manager.UnregisterLoader(&loader);
    manager.SetRegistry(nullptr);
}

TEST_CASE("Asset.Audio.EngineFormat.Load") {
    const u32 channels = 1;
    const u32 sampleRate = 48000;
    const u32 frameCount = 8;
    const u32 framesPerChunk = 4;
    const u32 chunkCount = 2;
    const u32 sampleFormat = AltinaEngine::Asset::kAudioSampleFormatPcm16;

    const u32 bytesPerSample = GetAudioBytesPerSample(sampleFormat);
    REQUIRE(bytesPerSample == 2U);

    const u32 dataSize = frameCount * channels * bytesPerSample;
    const u32 chunkTableBytes = chunkCount * sizeof(AltinaEngine::Asset::FAudioChunkDesc);

    AltinaEngine::Asset::FAudioBlobDesc blobDesc{};
    blobDesc.Codec            = AltinaEngine::Asset::kAudioCodecPcm;
    blobDesc.SampleFormat     = sampleFormat;
    blobDesc.Channels         = channels;
    blobDesc.SampleRate       = sampleRate;
    blobDesc.FrameCount       = frameCount;
    blobDesc.ChunkCount       = chunkCount;
    blobDesc.FramesPerChunk   = framesPerChunk;
    blobDesc.ChunkTableOffset = 0;
    blobDesc.DataOffset       = chunkTableBytes;
    blobDesc.DataSize         = dataSize;

    AltinaEngine::Asset::FAssetBlobHeader header{};
    header.Type     = static_cast<u8>(AltinaEngine::Asset::EAssetType::Audio);
    header.DescSize = static_cast<u32>(sizeof(AltinaEngine::Asset::FAudioBlobDesc));
    header.DataSize = blobDesc.DataOffset + blobDesc.DataSize;

    TVector<u8> cooked;
    cooked.Resize(sizeof(header) + sizeof(blobDesc) + header.DataSize);

    u8* writePtr = cooked.Data();
    std::memcpy(writePtr, &header, sizeof(header));
    writePtr += sizeof(header);
    std::memcpy(writePtr, &blobDesc, sizeof(blobDesc));
    writePtr += sizeof(blobDesc);

    AltinaEngine::Asset::FAudioChunkDesc chunks[2]{};
    chunks[0].Offset = blobDesc.DataOffset;
    chunks[0].Size   = dataSize / 2;
    chunks[1].Offset = blobDesc.DataOffset + chunks[0].Size;
    chunks[1].Size   = dataSize - chunks[0].Size;
    std::memcpy(writePtr + blobDesc.ChunkTableOffset, chunks, sizeof(chunks));

    const u16 samples[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    std::memcpy(writePtr + blobDesc.DataOffset, samples, sizeof(samples));

    FTestAssetStream stream(cooked);
    FAudioLoader loader;

    FAssetDesc desc{};
    desc.Audio.Codec      = AltinaEngine::Asset::kAudioCodecPcm;
    desc.Audio.Channels   = channels;
    desc.Audio.SampleRate = sampleRate;

    auto asset = loader.Load(desc, stream);
    REQUIRE(asset);

    auto* audio = static_cast<FAudioAsset*>(asset.Get());
    REQUIRE(audio != nullptr);

    const auto& runtime = audio->GetDesc();
    REQUIRE_EQ(runtime.Codec, blobDesc.Codec);
    REQUIRE_EQ(runtime.SampleFormat, blobDesc.SampleFormat);
    REQUIRE_EQ(runtime.Channels, blobDesc.Channels);
    REQUIRE_EQ(runtime.SampleRate, blobDesc.SampleRate);
    REQUIRE_EQ(runtime.FrameCount, blobDesc.FrameCount);
    REQUIRE_EQ(runtime.FramesPerChunk, blobDesc.FramesPerChunk);

    REQUIRE_EQ(audio->GetChunks().Size(), static_cast<usize>(chunkCount));
    REQUIRE_EQ(audio->GetData().Size(), static_cast<usize>(dataSize));
}

TEST_CASE("Asset.Audio.EngineFormat.LoadFromRegistry") {
    FAssetRegistry registry;
    const auto registryPath = GetRegistryPath();
    REQUIRE(registry.LoadFromJsonFile(ToFString(registryPath)));

    const auto cookedRoot = registryPath.parent_path().parent_path();
    FScopedCurrentPath scopedPath(cookedRoot);
    REQUIRE(scopedPath.Ok);

    FAssetManager manager;
    FAudioLoader loader;
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
    REQUIRE_EQ(runtime.Codec, registryDesc->Audio.Codec);
    REQUIRE_EQ(runtime.Channels, registryDesc->Audio.Channels);
    REQUIRE_EQ(runtime.SampleRate, registryDesc->Audio.SampleRate);
    REQUIRE(runtime.FrameCount > 0U);
    REQUIRE(!audio->GetData().IsEmpty());

    if (registryDesc->Audio.DurationSeconds > 0.0f) {
        const float duration = static_cast<float>(runtime.FrameCount)
            / static_cast<float>(runtime.SampleRate);
        REQUIRE(std::abs(duration - registryDesc->Audio.DurationSeconds) < 0.02f);
    }

    manager.UnregisterLoader(&loader);
    manager.SetRegistry(nullptr);
}
