
#include "Asset/AssetBinary.h"
#include "Asset/AssetBundle.h"
#include "Asset/AssetTypes.h"
#include "Container/Span.h"
#include "Container/Vector.h"
#include "Imaging/ImageIO.h"
#include "Types/Aliases.h"
#include "Utility/Json.h"
#include "Utility/Uuid.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace AltinaEngine::Tools::AssetPipeline {
    namespace Container = Core::Container;
    namespace {
        using Container::FNativeString;
        using Container::FNativeStringView;
        using Container::TSpan;
        using Container::TVector;
        using Core::Utility::Json::EJsonType;
        using Core::Utility::Json::FindObjectValueInsensitive;
        using Core::Utility::Json::FJsonDocument;
        using Core::Utility::Json::FJsonValue;
        using Core::Utility::Json::GetNumberValue;
        using Core::Utility::Json::GetStringValue;

        struct FToolPaths {
            std::filesystem::path Root;
            std::filesystem::path BuildRoot;
            std::filesystem::path CookedRoot;
            std::filesystem::path CacheRoot;
            std::filesystem::path CookCachePath;
        };

        struct FCommandLine {
            std::string                                  Command;
            std::unordered_map<std::string, std::string> Options;
        };

        struct FAssetRecord {
            std::filesystem::path SourcePath;
            std::filesystem::path MetaPath;
            std::string           SourcePathRel;
            std::string           VirtualPath;
            Asset::EAssetType     Type = Asset::EAssetType::Unknown;
            std::string           ImporterName;
            u32                   ImporterVersion = 1U;
            FUuid                 Uuid;
        };

        struct FRegistryEntry {
            std::string           Uuid;
            Asset::EAssetType     Type = Asset::EAssetType::Unknown;
            std::string           VirtualPath;
            std::string           CookedPath;
            Asset::FTexture2DDesc TextureDesc{};
            bool                  HasTextureDesc = false;
            Asset::FMeshDesc      MeshDesc{};
            bool                  HasMeshDesc = false;
            Asset::FAudioDesc     AudioDesc{};
            bool                  HasAudioDesc = false;
        };

        struct FCookCacheEntry {
            std::string Uuid;
            std::string CookKey;
            std::string SourcePath;
            std::string CookedPath;
            std::string LastCooked;
        };

        enum class EMetaWriteMode {
            MissingOnly,
            Always
        };

        constexpr u32 kCookPipelineVersion = 4;
        constexpr u64 kFnvOffsetBasis      = 14695981039346656037ULL;
        constexpr u64 kFnvPrime            = 1099511628211ULL;
        auto          ToLowerAscii(char value) -> char {
            if (value >= 'A' && value <= 'Z') {
                return static_cast<char>(value - 'A' + 'a');
            }
            return value;
        }

        void ToLowerAscii(std::string& value) {
            for (char& character : value) {
                character = ToLowerAscii(character);
            }
        }

        auto EscapeJson(const std::string& value) -> std::string {
            std::string out;
            out.reserve(value.size() + 8U);
            for (char character : value) {
                switch (character) {
                    case '\\':
                        out += "\\\\";
                        break;
                    case '"':
                        out += "\\\"";
                        break;
                    case '\n':
                        out += "\\n";
                        break;
                    case '\r':
                        out += "\\r";
                        break;
                    case '\t':
                        out += "\\t";
                        break;
                    default:
                        if (static_cast<unsigned char>(character) < 0x20U) {
                            std::ostringstream stream;
                            stream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                                   << static_cast<int>(static_cast<unsigned char>(character));
                            out += stream.str();
                        } else {
                            out += character;
                        }
                        break;
                }
            }
            return out;
        }

        auto NormalizePath(const std::filesystem::path& path) -> std::string {
            std::string out = path.generic_string();
            if (out.size() >= 2U && out[0] == '.' && out[1] == '/') {
                out.erase(0, 2U);
            }
            return out;
        }

        auto MakeRelativePath(const std::filesystem::path& root, const std::filesystem::path& path)
            -> std::string {
            std::error_code       ec;
            std::filesystem::path rel = std::filesystem::relative(path, root, ec);
            if (ec) {
                return NormalizePath(path);
            }
            return NormalizePath(rel);
        }

        auto ToStdString(const FNativeString& value) -> std::string {
            if (value.IsEmptyString()) {
                return {};
            }
            return std::string(value.GetData(), value.Length());
        }
        auto AssetTypeToString(Asset::EAssetType type) -> const char* {
            switch (type) {
                case Asset::EAssetType::Texture2D:
                    return "Texture2D";
                case Asset::EAssetType::Mesh:
                    return "Mesh";
                case Asset::EAssetType::Material:
                    return "Material";
                case Asset::EAssetType::Audio:
                    return "Audio";
                case Asset::EAssetType::Redirector:
                    return "Redirector";
                default:
                    return "Unknown";
            }
        }

        auto ParseAssetType(std::string value) -> Asset::EAssetType {
            ToLowerAscii(value);
            if (value == "texture2d") {
                return Asset::EAssetType::Texture2D;
            }
            if (value == "mesh") {
                return Asset::EAssetType::Mesh;
            }
            if (value == "material") {
                return Asset::EAssetType::Material;
            }
            if (value == "audio") {
                return Asset::EAssetType::Audio;
            }
            if (value == "redirector") {
                return Asset::EAssetType::Redirector;
            }
            return Asset::EAssetType::Unknown;
        }

        auto GetImporterName(Asset::EAssetType type) -> std::string {
            switch (type) {
                case Asset::EAssetType::Texture2D:
                    return "TextureImporter";
                case Asset::EAssetType::Mesh:
                    return "MeshImporter";
                case Asset::EAssetType::Audio:
                    return "AudioImporter";
                default:
                    return "UnknownImporter";
            }
        }

        auto IsTextureExtension(const std::filesystem::path& path) -> bool {
            std::string ext = path.extension().string();
            ToLowerAscii(ext);
            return ext == ".png" || ext == ".jpg" || ext == ".jpeg";
        }

        auto IsMeshExtension(const std::filesystem::path& path) -> bool {
            std::string ext = path.extension().string();
            ToLowerAscii(ext);
            return ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb";
        }

        auto IsAudioExtension(const std::filesystem::path& path) -> bool {
            std::string ext = path.extension().string();
            ToLowerAscii(ext);
            return ext == ".wav" || ext == ".ogg";
        }

        auto GuessAssetType(const std::filesystem::path& path) -> Asset::EAssetType {
            if (IsTextureExtension(path)) {
                return Asset::EAssetType::Texture2D;
            }
            if (IsMeshExtension(path)) {
                return Asset::EAssetType::Mesh;
            }
            if (IsAudioExtension(path)) {
                return Asset::EAssetType::Audio;
            }
            return Asset::EAssetType::Unknown;
        }
        void HashBytes(u64& hash, const void* data, size_t size) {
            if (data == nullptr || size == 0U) {
                return;
            }

            const auto* bytes = static_cast<const u8*>(data);
            for (size_t i = 0; i < size; ++i) {
                hash ^= static_cast<u64>(bytes[i]);
                hash *= kFnvPrime;
            }
        }

        void HashString(u64& hash, const std::string& value) {
            HashBytes(hash, value.data(), value.size());
        }

        auto FormatHex64(u64 value) -> std::string {
            std::ostringstream stream;
            stream << std::hex << std::nouppercase << std::setw(16) << std::setfill('0') << value;
            return stream.str();
        }

        auto BuildCookKey(const std::vector<u8>& sourceBytes, const FAssetRecord& asset,
            const std::string& platform) -> std::string {
            u64 hash = kFnvOffsetBasis;
            HashBytes(hash, &kCookPipelineVersion, sizeof(kCookPipelineVersion));
            HashBytes(hash, sourceBytes.data(), sourceBytes.size());
            HashString(hash, asset.ImporterName);
            HashBytes(hash, &asset.ImporterVersion, sizeof(asset.ImporterVersion));
            const u8 typeValue = static_cast<u8>(asset.Type);
            HashBytes(hash, &typeValue, sizeof(typeValue));
            HashString(hash, platform);
            return "fnv1a64:" + FormatHex64(hash);
        }

        auto BuildCookKeyWithExtras(const std::vector<u8>& sourceBytes,
            const std::vector<u8>& extraBytes, const FAssetRecord& asset,
            const std::string& platform) -> std::string {
            u64 hash = kFnvOffsetBasis;
            HashBytes(hash, &kCookPipelineVersion, sizeof(kCookPipelineVersion));
            HashBytes(hash, sourceBytes.data(), sourceBytes.size());
            if (!extraBytes.empty()) {
                HashBytes(hash, extraBytes.data(), extraBytes.size());
            }
            HashString(hash, asset.ImporterName);
            HashBytes(hash, &asset.ImporterVersion, sizeof(asset.ImporterVersion));
            const u8 typeValue = static_cast<u8>(asset.Type);
            HashBytes(hash, &typeValue, sizeof(typeValue));
            HashString(hash, platform);
            return "fnv1a64:" + FormatHex64(hash);
        }

        auto GetUtcTimestamp() -> std::string {
            using clock                 = std::chrono::system_clock;
            const auto        now       = clock::now();
            const std::time_t timeValue = clock::to_time_t(now);
            std::tm           utc{};
#if defined(_WIN32)
            gmtime_s(&utc, &timeValue);
#else
            gmtime_r(&timeValue, &utc);
#endif
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min,
                utc.tm_sec);
            return std::string(buffer);
        }
        auto ReadFileText(const std::filesystem::path& path, std::string& outText) -> bool {
            std::ifstream file(path, std::ios::binary);
            if (!file) {
                return false;
            }

            std::ostringstream stream;
            stream << file.rdbuf();
            outText = stream.str();
            return true;
        }

        auto ReadFileBytes(const std::filesystem::path& path, std::vector<u8>& outBytes) -> bool {
            std::ifstream file(path, std::ios::binary);
            if (!file) {
                return false;
            }

            file.seekg(0, std::ios::end);
            const auto endPos = file.tellg();
            if (endPos < 0) {
                return false;
            }
            const auto size = static_cast<size_t>(endPos);
            file.seekg(0, std::ios::beg);

            outBytes.resize(size);
            if (size > 0U) {
                file.read(
                    reinterpret_cast<char*>(outBytes.data()), static_cast<std::streamsize>(size));
            }
            return file.good() || file.eof();
        }

        auto WriteTextFile(const std::filesystem::path& path, const std::string& text) -> bool {
            std::ofstream file(path, std::ios::binary);
            if (!file) {
                return false;
            }
            if (!text.empty()) {
                file.write(text.data(), static_cast<std::streamsize>(text.size()));
            }
            return file.good();
        }

        auto WriteBytesFile(const std::filesystem::path& path, const std::vector<u8>& bytes)
            -> bool {
            std::ofstream file(path, std::ios::binary);
            if (!file) {
                return false;
            }
            if (!bytes.empty()) {
                file.write(reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
            }
            return file.good();
        }

        auto DecodeImageBytes(const std::vector<u8>& sourceBytes, Imaging::FImage& outImage)
            -> bool {
            if (sourceBytes.empty()) {
                return false;
            }

            TVector<u8> bytes;
            bytes.Resize(static_cast<usize>(sourceBytes.size()));
            std::memcpy(bytes.Data(), sourceBytes.data(), sourceBytes.size());

            const TSpan<u8>          span(bytes);
            Imaging::FPngImageReader pngReader;
            if (pngReader.CanRead(span)) {
                return pngReader.Read(span, outImage);
            }

            Imaging::FJpegImageReader jpegReader;
            if (jpegReader.CanRead(span)) {
                return jpegReader.Read(span, outImage);
            }

            return false;
        }

        auto CookTexture2D(const std::vector<u8>& sourceBytes, bool srgb,
            std::vector<u8>& outCooked, Asset::FTexture2DDesc& outDesc) -> bool {
            Imaging::FImage image;
            if (!DecodeImageBytes(sourceBytes, image) || !image.IsValid()) {
                return false;
            }

            if (image.GetFormat() == Imaging::EImageFormat::Unknown) {
                return false;
            }

            const auto dataSize = image.GetDataSize();
            if (dataSize == 0U || dataSize > std::numeric_limits<u32>::max()) {
                return false;
            }

            Asset::FTexture2DBlobDesc blobDesc{};
            blobDesc.Width    = image.GetWidth();
            blobDesc.Height   = image.GetHeight();
            blobDesc.Format   = static_cast<u32>(image.GetFormat());
            blobDesc.MipCount = 1;
            blobDesc.RowPitch = image.GetRowPitch();

            const u32 bytesPerPixel = Asset::GetTextureBytesPerPixel(blobDesc.Format);
            if (bytesPerPixel == 0 || blobDesc.RowPitch < blobDesc.Width * bytesPerPixel) {
                return false;
            }

            const u64 expectedSize = static_cast<u64>(blobDesc.RowPitch) * blobDesc.Height;
            if (expectedSize != dataSize) {
                return false;
            }

            Asset::FAssetBlobHeader header{};
            header.Type     = static_cast<u8>(Asset::EAssetType::Texture2D);
            header.Flags    = Asset::MakeAssetBlobFlags(srgb);
            header.DescSize = static_cast<u32>(sizeof(Asset::FTexture2DBlobDesc));
            header.DataSize = static_cast<u32>(dataSize);

            const usize totalSize =
                sizeof(Asset::FAssetBlobHeader) + sizeof(Asset::FTexture2DBlobDesc) + dataSize;
            outCooked.resize(totalSize);

            std::memcpy(outCooked.data(), &header, sizeof(Asset::FAssetBlobHeader));
            std::memcpy(outCooked.data() + sizeof(Asset::FAssetBlobHeader), &blobDesc,
                sizeof(Asset::FTexture2DBlobDesc));
            std::memcpy(outCooked.data() + sizeof(Asset::FAssetBlobHeader)
                    + sizeof(Asset::FTexture2DBlobDesc),
                image.GetData(), dataSize);

            outDesc.Width    = blobDesc.Width;
            outDesc.Height   = blobDesc.Height;
            outDesc.Format   = blobDesc.Format;
            outDesc.MipCount = blobDesc.MipCount;
            outDesc.SRGB     = srgb;

            return true;
        }

        auto ReadU16LE(const std::vector<u8>& bytes, size_t offset, u16& out) -> bool {
            if (offset + sizeof(u16) > bytes.size()) {
                return false;
            }
            std::memcpy(&out, bytes.data() + offset, sizeof(u16));
            return true;
        }

        auto ReadU32LE(const std::vector<u8>& bytes, size_t offset, u32& out) -> bool {
            if (offset + sizeof(u32) > bytes.size()) {
                return false;
            }
            std::memcpy(&out, bytes.data() + offset, sizeof(u32));
            return true;
        }

        auto ReadU64LE(const std::vector<u8>& bytes, size_t offset, u64& out) -> bool {
            if (offset + sizeof(u64) > bytes.size()) {
                return false;
            }
            std::memcpy(&out, bytes.data() + offset, sizeof(u64));
            return true;
        }

        auto MatchTag(const std::vector<u8>& bytes, size_t offset, const char* tag) -> bool {
            if (offset + 4U > bytes.size()) {
                return false;
            }
            return std::memcmp(bytes.data() + offset, tag, 4U) == 0;
        }

        struct FWavInfo {
            u32             Channels     = 0;
            u32             SampleRate   = 0;
            u32             SampleFormat = 0;
            u32             FrameCount   = 0;
            std::vector<u8> Data;
        };

        auto ParseWav(const std::vector<u8>& bytes, FWavInfo& outInfo) -> bool {
            if (bytes.size() < 12U) {
                return false;
            }
            if (!MatchTag(bytes, 0, "RIFF")) {
                return false;
            }
            if (!MatchTag(bytes, 8, "WAVE")) {
                return false;
            }

            size_t          offset        = 12U;
            bool            hasFmt        = false;
            bool            hasData       = false;
            u16             audioFormat   = 0;
            u16             channels      = 0;
            u32             sampleRate    = 0;
            u16             blockAlign    = 0;
            u16             bitsPerSample = 0;
            std::vector<u8> data;

            while (offset + 8U <= bytes.size()) {
                u32 chunkSize = 0;
                if (!ReadU32LE(bytes, offset + 4U, chunkSize)) {
                    return false;
                }
                const size_t chunkDataOffset = offset + 8U;
                if (chunkDataOffset + chunkSize > bytes.size()) {
                    return false;
                }

                if (MatchTag(bytes, offset, "fmt ")) {
                    if (chunkSize < 16U) {
                        return false;
                    }
                    if (!ReadU16LE(bytes, chunkDataOffset + 0U, audioFormat)) {
                        return false;
                    }
                    if (!ReadU16LE(bytes, chunkDataOffset + 2U, channels)) {
                        return false;
                    }
                    if (!ReadU32LE(bytes, chunkDataOffset + 4U, sampleRate)) {
                        return false;
                    }
                    if (!ReadU16LE(bytes, chunkDataOffset + 12U, blockAlign)) {
                        return false;
                    }
                    if (!ReadU16LE(bytes, chunkDataOffset + 14U, bitsPerSample)) {
                        return false;
                    }

                    if (audioFormat == 0xFFFE) {
                        if (chunkSize < 40U) {
                            return false;
                        }
                        u16 cbSize = 0;
                        if (!ReadU16LE(bytes, chunkDataOffset + 16U, cbSize)) {
                            return false;
                        }
                        if (cbSize < 22U) {
                            return false;
                        }
                        u32 subFormat = 0;
                        if (!ReadU32LE(bytes, chunkDataOffset + 24U, subFormat)) {
                            return false;
                        }
                        if (subFormat == 0x00000001u) {
                            audioFormat = 1;
                        } else if (subFormat == 0x00000003u) {
                            audioFormat = 3;
                        } else {
                            return false;
                        }
                    }

                    hasFmt = true;
                } else if (MatchTag(bytes, offset, "data")) {
                    data.assign(bytes.begin() + static_cast<std::ptrdiff_t>(chunkDataOffset),
                        bytes.begin() + static_cast<std::ptrdiff_t>(chunkDataOffset + chunkSize));
                    hasData = true;
                }

                offset = chunkDataOffset + chunkSize;
                if ((chunkSize & 1U) != 0U) {
                    ++offset;
                }
            }

            if (!hasFmt || !hasData) {
                return false;
            }
            if (channels == 0U || sampleRate == 0U) {
                return false;
            }

            u32 sampleFormat   = Asset::kAudioSampleFormatUnknown;
            u32 bytesPerSample = 0;
            if (audioFormat == 1) {
                if (bitsPerSample != 16U) {
                    return false;
                }
                sampleFormat   = Asset::kAudioSampleFormatPcm16;
                bytesPerSample = 2;
            } else if (audioFormat == 3) {
                if (bitsPerSample != 32U) {
                    return false;
                }
                sampleFormat   = Asset::kAudioSampleFormatPcm32f;
                bytesPerSample = 4;
            } else {
                return false;
            }

            const u32 bytesPerFrame = static_cast<u32>(channels) * bytesPerSample;
            if (bytesPerFrame == 0U || blockAlign == 0U || blockAlign != bytesPerFrame) {
                return false;
            }

            if (data.empty()) {
                return false;
            }
            if ((data.size() % bytesPerFrame) != 0U) {
                return false;
            }

            const u64 frameCount64 = data.size() / bytesPerFrame;
            if (frameCount64 == 0U || frameCount64 > std::numeric_limits<u32>::max()) {
                return false;
            }

            outInfo.Channels     = channels;
            outInfo.SampleRate   = sampleRate;
            outInfo.SampleFormat = sampleFormat;
            outInfo.FrameCount   = static_cast<u32>(frameCount64);
            outInfo.Data         = std::move(data);
            return true;
        }

        struct FOggInfo {
            u32 Channels   = 0;
            u32 SampleRate = 0;
            u64 FrameCount = 0;
        };

        auto ParseVorbisIdPacket(const std::vector<u8>& packet, FOggInfo& outInfo) -> bool {
            if (packet.size() < 30U) {
                return false;
            }
            if (packet[0] != 0x01) {
                return false;
            }
            if (std::memcmp(packet.data() + 1, "vorbis", 6U) != 0) {
                return false;
            }
            u32 version = 0;
            std::memcpy(&version, packet.data() + 7, sizeof(u32));
            if (version != 0U) {
                return false;
            }
            const u8 channels   = packet[11];
            u32      sampleRate = 0;
            std::memcpy(&sampleRate, packet.data() + 12, sizeof(u32));
            if (channels == 0U || sampleRate == 0U) {
                return false;
            }
            outInfo.Channels   = channels;
            outInfo.SampleRate = sampleRate;
            return true;
        }

        auto ParseOggVorbis(const std::vector<u8>& bytes, FOggInfo& outInfo) -> bool {
            if (bytes.size() < 27U) {
                return false;
            }

            size_t          offset = 0U;
            bool            gotId  = false;
            std::vector<u8> packet;
            u64             lastGranule = 0U;
            bool            hasGranule  = false;
            u32             serial      = 0U;
            bool            serialSet   = false;

            while (offset + 27U <= bytes.size()) {
                if (!MatchTag(bytes, offset, "OggS")) {
                    return false;
                }
                const u8 version = bytes[offset + 4U];
                if (version != 0U) {
                    return false;
                }
                u64 granule = 0;
                if (!ReadU64LE(bytes, offset + 6U, granule)) {
                    return false;
                }
                u32 pageSerial = 0;
                if (!ReadU32LE(bytes, offset + 14U, pageSerial)) {
                    return false;
                }
                if (!serialSet) {
                    serial    = pageSerial;
                    serialSet = true;
                } else if (pageSerial != serial) {
                    return false;
                }

                const u8     segmentCount  = bytes[offset + 26U];
                const size_t segmentOffset = offset + 27U;
                const size_t dataOffset    = segmentOffset + segmentCount;
                if (dataOffset > bytes.size()) {
                    return false;
                }
                size_t totalSegSize = 0U;
                for (u32 i = 0; i < segmentCount; ++i) {
                    totalSegSize += bytes[segmentOffset + i];
                }
                if (dataOffset + totalSegSize > bytes.size()) {
                    return false;
                }

                if (granule != 0xFFFFFFFFFFFFFFFFULL) {
                    lastGranule = granule;
                    hasGranule  = true;
                }

                if (!gotId) {
                    size_t dataPos = dataOffset;
                    for (u32 i = 0; i < segmentCount; ++i) {
                        const u8 segSize = bytes[segmentOffset + i];
                        if (segSize > 0U) {
                            packet.insert(packet.end(),
                                bytes.begin() + static_cast<std::ptrdiff_t>(dataPos),
                                bytes.begin() + static_cast<std::ptrdiff_t>(dataPos + segSize));
                        }
                        dataPos += segSize;
                        if (segSize < 255U) {
                            if (!ParseVorbisIdPacket(packet, outInfo)) {
                                return false;
                            }
                            gotId = true;
                            packet.clear();
                            break;
                        }
                    }
                }

                offset = dataOffset + totalSegSize;
            }

            if (!gotId || !hasGranule) {
                return false;
            }
            if (lastGranule == 0U || lastGranule > std::numeric_limits<u32>::max()) {
                return false;
            }

            outInfo.FrameCount = lastGranule;
            return true;
        }

        auto BuildAudioBlob(const std::vector<u8>& data, u32 codec, u32 sampleFormat, u32 channels,
            u32 sampleRate, u32 frameCount, u32 framesPerChunk,
            const std::vector<Asset::FAudioChunkDesc>& chunks, std::vector<u8>& outCooked) -> bool {
            if (channels == 0U || sampleRate == 0U || frameCount == 0U) {
                return false;
            }
            if (framesPerChunk == 0U || chunks.empty()) {
                return false;
            }
            if (data.empty()) {
                return false;
            }

            const u64 dataSize = data.size();
            if (dataSize > std::numeric_limits<u32>::max()) {
                return false;
            }

            const u64 chunkTableBytes =
                static_cast<u64>(chunks.size()) * sizeof(Asset::FAudioChunkDesc);
            if (chunkTableBytes > std::numeric_limits<u32>::max()) {
                return false;
            }

            const u64 blobDataSize = chunkTableBytes + dataSize;
            if (blobDataSize > std::numeric_limits<u32>::max()) {
                return false;
            }

            Asset::FAssetBlobHeader header{};
            header.Type     = static_cast<u8>(Asset::EAssetType::Audio);
            header.DescSize = static_cast<u32>(sizeof(Asset::FAudioBlobDesc));
            header.DataSize = static_cast<u32>(blobDataSize);

            Asset::FAudioBlobDesc blobDesc{};
            blobDesc.Codec            = codec;
            blobDesc.SampleFormat     = sampleFormat;
            blobDesc.Channels         = channels;
            blobDesc.SampleRate       = sampleRate;
            blobDesc.FrameCount       = frameCount;
            blobDesc.ChunkCount       = static_cast<u32>(chunks.size());
            blobDesc.FramesPerChunk   = framesPerChunk;
            blobDesc.ChunkTableOffset = 0U;
            blobDesc.DataOffset       = static_cast<u32>(chunkTableBytes);
            blobDesc.DataSize         = static_cast<u32>(dataSize);

            const usize totalSize =
                sizeof(Asset::FAssetBlobHeader) + sizeof(Asset::FAudioBlobDesc) + header.DataSize;
            outCooked.resize(totalSize);

            u8* writePtr = outCooked.data();
            std::memcpy(writePtr, &header, sizeof(header));
            writePtr += sizeof(header);
            std::memcpy(writePtr, &blobDesc, sizeof(blobDesc));
            writePtr += sizeof(blobDesc);

            if (!chunks.empty()) {
                std::memcpy(writePtr + blobDesc.ChunkTableOffset, chunks.data(),
                    chunks.size() * sizeof(Asset::FAudioChunkDesc));
            }
            std::memcpy(writePtr + blobDesc.DataOffset, data.data(), dataSize);
            return true;
        }

        auto CookAudio(const std::filesystem::path& sourcePath, const std::vector<u8>& sourceBytes,
            std::vector<u8>& outCooked, Asset::FAudioDesc& outDesc) -> bool {
            std::string ext = sourcePath.extension().string();
            ToLowerAscii(ext);

            u32             codec        = 0;
            u32             sampleFormat = 0;
            u32             channels     = 0;
            u32             sampleRate   = 0;
            u32             frameCount   = 0;
            std::vector<u8> data;

            if (ext == ".wav") {
                FWavInfo wav{};
                if (!ParseWav(sourceBytes, wav)) {
                    return false;
                }
                codec        = Asset::kAudioCodecPcm;
                sampleFormat = wav.SampleFormat;
                channels     = wav.Channels;
                sampleRate   = wav.SampleRate;
                frameCount   = wav.FrameCount;
                data         = std::move(wav.Data);
            } else if (ext == ".ogg") {
                FOggInfo ogg{};
                if (!ParseOggVorbis(sourceBytes, ogg)) {
                    return false;
                }
                codec        = Asset::kAudioCodecOggVorbis;
                sampleFormat = Asset::kAudioSampleFormatPcm16;
                channels     = ogg.Channels;
                sampleRate   = ogg.SampleRate;
                if (ogg.FrameCount > std::numeric_limits<u32>::max()) {
                    return false;
                }
                frameCount = static_cast<u32>(ogg.FrameCount);
                data       = sourceBytes;
            } else {
                return false;
            }

            if (channels == 0U || sampleRate == 0U || frameCount == 0U) {
                return false;
            }

            std::vector<Asset::FAudioChunkDesc> chunks;
            u32                                 framesPerChunk = 0;
            if (codec == Asset::kAudioCodecPcm) {
                const u32 bytesPerSample = Asset::GetAudioBytesPerSample(sampleFormat);
                if (bytesPerSample == 0U) {
                    return false;
                }
                const u32 bytesPerFrame = channels * bytesPerSample;
                if (bytesPerFrame == 0U) {
                    return false;
                }

                constexpr u32 kTargetFramesPerChunk = 4096;
                framesPerChunk =
                    frameCount < kTargetFramesPerChunk ? frameCount : kTargetFramesPerChunk;
                const u32 chunkCount = (frameCount + framesPerChunk - 1U) / framesPerChunk;

                if (chunkCount == 0U) {
                    return false;
                }

                chunks.reserve(chunkCount);
                u32 remainingFrames = frameCount;
                for (u32 chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex) {
                    const u32 takeFrames =
                        remainingFrames < framesPerChunk ? remainingFrames : framesPerChunk;
                    const u32              chunkBytes = takeFrames * bytesPerFrame;
                    Asset::FAudioChunkDesc chunk{};
                    chunk.Offset = 0U; // filled after table size known.
                    chunk.Size   = chunkBytes;
                    chunks.push_back(chunk);
                    remainingFrames -= takeFrames;
                }
            } else {
                constexpr u32 kTargetChunkBytes = 64U * 1024U;
                const u32     dataSize          = static_cast<u32>(data.size());
                u32           chunkCount = (dataSize + kTargetChunkBytes - 1U) / kTargetChunkBytes;
                if (chunkCount == 0U) {
                    chunkCount = 1U;
                }
                framesPerChunk = (frameCount + chunkCount - 1U) / chunkCount;
                if (framesPerChunk == 0U) {
                    return false;
                }

                chunks.reserve(chunkCount);
                u32 remainingBytes = dataSize;
                for (u32 chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex) {
                    const u32 takeBytes =
                        remainingBytes < kTargetChunkBytes ? remainingBytes : kTargetChunkBytes;
                    Asset::FAudioChunkDesc chunk{};
                    chunk.Offset = 0U; // filled after table size known.
                    chunk.Size   = takeBytes;
                    chunks.push_back(chunk);
                    remainingBytes -= takeBytes;
                }
            }

            const u64 chunkTableBytes =
                static_cast<u64>(chunks.size()) * sizeof(Asset::FAudioChunkDesc);
            if (chunkTableBytes > std::numeric_limits<u32>::max()) {
                return false;
            }
            const u32 dataOffset    = static_cast<u32>(chunkTableBytes);
            u32       runningOffset = 0U;
            for (auto& chunk : chunks) {
                chunk.Offset = dataOffset + runningOffset;
                runningOffset += chunk.Size;
            }
            if (runningOffset != static_cast<u32>(data.size())) {
                return false;
            }

            if (!BuildAudioBlob(data, codec, sampleFormat, channels, sampleRate, frameCount,
                    framesPerChunk, chunks, outCooked)) {
                return false;
            }

            outDesc.Codec      = codec;
            outDesc.Channels   = channels;
            outDesc.SampleRate = sampleRate;
            outDesc.DurationSeconds =
                static_cast<f32>(static_cast<double>(frameCount) / static_cast<double>(sampleRate));
            return true;
        }

        // Mesh cook helpers (engine format).
        struct FVec2 {
            float X = 0.0f;
            float Y = 0.0f;
        };

        struct FVec3 {
            float X = 0.0f;
            float Y = 0.0f;
            float Z = 0.0f;
        };

        struct FMeshBuildResult {
            std::vector<u8>                              VertexData;
            std::vector<u8>                              IndexData;
            std::vector<Asset::FMeshVertexAttributeDesc> Attributes;
            std::vector<Asset::FMeshSubMeshDesc>         SubMeshes;
            u32                                          VertexCount  = 0;
            u32                                          IndexCount   = 0;
            u32                                          VertexStride = 0;
            u32                                          IndexType    = Asset::kMeshIndexTypeUint32;
            u32                                          VertexFormatMask = 0;
            float                                        BoundsMin[3]     = { 0.0f, 0.0f, 0.0f };
            float                                        BoundsMax[3]     = { 0.0f, 0.0f, 0.0f };
        };

        struct FObjIndex {
            int  V  = -1;
            int  Vt = -1;
            int  Vn = -1;

            auto operator==(const FObjIndex& other) const noexcept -> bool {
                return V == other.V && Vt == other.Vt && Vn == other.Vn;
            }
        };

        struct FObjIndexHash {
            auto operator()(const FObjIndex& key) const noexcept -> size_t {
                const auto hv = static_cast<size_t>(key.V + 1);
                const auto ht = static_cast<size_t>(key.Vt + 1);
                const auto hn = static_cast<size_t>(key.Vn + 1);
                return (hv * 73856093u) ^ (ht * 19349663u) ^ (hn * 83492791u);
            }
        };

        auto FixObjIndex(int idx, size_t count) -> int {
            if (idx > 0) {
                return idx - 1;
            }
            if (idx < 0) {
                const int fixed = static_cast<int>(count) + idx;
                return fixed >= 0 ? fixed : -1;
            }
            return -1;
        }

        auto ParseObjIndexToken(const std::string& token, size_t vCount, size_t vtCount,
            size_t vnCount, FObjIndex& out) -> bool {
            if (token.empty()) {
                return false;
            }

            int    v  = 0;
            int    vt = 0;
            int    vn = 0;

            size_t firstSlash = token.find('/');
            if (firstSlash == std::string::npos) {
                v = std::stoi(token);
            } else {
                v                  = std::stoi(token.substr(0, firstSlash));
                size_t secondSlash = token.find('/', firstSlash + 1);
                if (secondSlash == std::string::npos) {
                    const auto vtText = token.substr(firstSlash + 1);
                    if (!vtText.empty()) {
                        vt = std::stoi(vtText);
                    }
                } else {
                    const auto vtText = token.substr(firstSlash + 1, secondSlash - firstSlash - 1);
                    if (!vtText.empty()) {
                        vt = std::stoi(vtText);
                    }
                    const auto vnText = token.substr(secondSlash + 1);
                    if (!vnText.empty()) {
                        vn = std::stoi(vnText);
                    }
                }
            }

            out.V  = FixObjIndex(v, vCount);
            out.Vt = FixObjIndex(vt, vtCount);
            out.Vn = FixObjIndex(vn, vnCount);
            return out.V >= 0;
        }

        auto BuildMeshBlob(const FMeshBuildResult& mesh, std::vector<u8>& outCooked,
            Asset::FMeshDesc& outDesc) -> bool {
            if (mesh.VertexCount == 0 || mesh.IndexCount == 0 || mesh.VertexStride == 0) {
                return false;
            }

            Asset::FMeshBlobDesc blobDesc{};
            blobDesc.VertexCount    = mesh.VertexCount;
            blobDesc.IndexCount     = mesh.IndexCount;
            blobDesc.VertexStride   = mesh.VertexStride;
            blobDesc.IndexType      = mesh.IndexType;
            blobDesc.AttributeCount = static_cast<u32>(mesh.Attributes.size());
            blobDesc.SubMeshCount   = static_cast<u32>(mesh.SubMeshes.size());
            blobDesc.VertexDataSize = static_cast<u32>(mesh.VertexData.size());
            blobDesc.IndexDataSize  = static_cast<u32>(mesh.IndexData.size());
            blobDesc.BoundsMin[0]   = mesh.BoundsMin[0];
            blobDesc.BoundsMin[1]   = mesh.BoundsMin[1];
            blobDesc.BoundsMin[2]   = mesh.BoundsMin[2];
            blobDesc.BoundsMax[0]   = mesh.BoundsMax[0];
            blobDesc.BoundsMax[1]   = mesh.BoundsMax[1];
            blobDesc.BoundsMax[2]   = mesh.BoundsMax[2];
            blobDesc.Flags          = 1U;

            const u32 attrBytes = blobDesc.AttributeCount * sizeof(Asset::FMeshVertexAttributeDesc);
            const u32 subMeshBytes = blobDesc.SubMeshCount * sizeof(Asset::FMeshSubMeshDesc);

            blobDesc.AttributesOffset = 0;
            blobDesc.SubMeshesOffset  = blobDesc.AttributesOffset + attrBytes;
            blobDesc.VertexDataOffset = blobDesc.SubMeshesOffset + subMeshBytes;
            blobDesc.IndexDataOffset  = blobDesc.VertexDataOffset + blobDesc.VertexDataSize;

            const u32               dataSize = blobDesc.IndexDataOffset + blobDesc.IndexDataSize;

            Asset::FAssetBlobHeader header{};
            header.Type     = static_cast<u8>(Asset::EAssetType::Mesh);
            header.DescSize = static_cast<u32>(sizeof(Asset::FMeshBlobDesc));
            header.DataSize = dataSize;

            const usize totalSize =
                sizeof(Asset::FAssetBlobHeader) + sizeof(Asset::FMeshBlobDesc) + dataSize;
            outCooked.resize(totalSize);

            u8* writePtr = outCooked.data();
            std::memcpy(writePtr, &header, sizeof(header));
            writePtr += sizeof(header);
            std::memcpy(writePtr, &blobDesc, sizeof(blobDesc));
            writePtr += sizeof(blobDesc);

            if (!mesh.Attributes.empty()) {
                std::memcpy(
                    writePtr + blobDesc.AttributesOffset, mesh.Attributes.data(), attrBytes);
            }
            if (!mesh.SubMeshes.empty()) {
                std::memcpy(
                    writePtr + blobDesc.SubMeshesOffset, mesh.SubMeshes.data(), subMeshBytes);
            }
            if (!mesh.VertexData.empty()) {
                std::memcpy(writePtr + blobDesc.VertexDataOffset, mesh.VertexData.data(),
                    mesh.VertexData.size());
            }
            if (!mesh.IndexData.empty()) {
                std::memcpy(writePtr + blobDesc.IndexDataOffset, mesh.IndexData.data(),
                    mesh.IndexData.size());
            }

            outDesc.VertexFormat = mesh.VertexFormatMask;
            outDesc.IndexFormat  = mesh.IndexType;
            outDesc.SubMeshCount = blobDesc.SubMeshCount;

            return true;
        }

        auto CookMeshFromObj(const std::filesystem::path& sourcePath, FMeshBuildResult& outMesh)
            -> bool {
            std::ifstream file(sourcePath);
            if (!file) {
                return false;
            }

            std::vector<FVec3>                                positions;
            std::vector<FVec3>                                normals;
            std::vector<FVec2>                                texcoords;

            std::vector<FVec3>                                outPositions;
            std::vector<FVec3>                                outNormals;
            std::vector<FVec2>                                outTexcoords;
            std::vector<u32>                                  indices;
            std::unordered_map<FObjIndex, u32, FObjIndexHash> indexMap;

            bool                                              hasNormal   = false;
            bool                                              hasTexcoord = false;
            bool                                              boundsSet   = false;

            std::string                                       line;
            while (std::getline(file, line)) {
                if (line.empty()) {
                    continue;
                }
                std::istringstream stream(line);
                std::string        tag;
                stream >> tag;
                if (tag == "v") {
                    FVec3 v{};
                    stream >> v.X >> v.Y >> v.Z;
                    positions.push_back(v);
                    continue;
                }
                if (tag == "vn") {
                    FVec3 n{};
                    stream >> n.X >> n.Y >> n.Z;
                    normals.push_back(n);
                    continue;
                }
                if (tag == "vt") {
                    FVec2 t{};
                    stream >> t.X >> t.Y;
                    texcoords.push_back(t);
                    continue;
                }
                if (tag != "f") {
                    continue;
                }

                std::vector<FObjIndex> face;
                std::string            token;
                while (stream >> token) {
                    FObjIndex idx{};
                    if (!ParseObjIndexToken(
                            token, positions.size(), texcoords.size(), normals.size(), idx)) {
                        return false;
                    }
                    if (idx.Vt >= 0) {
                        hasTexcoord = true;
                    }
                    if (idx.Vn >= 0) {
                        hasNormal = true;
                    }
                    face.push_back(idx);
                }

                if (face.size() < 3) {
                    continue;
                }

                auto emitVertex = [&](const FObjIndex& idx) -> u32 {
                    auto it = indexMap.find(idx);
                    if (it != indexMap.end()) {
                        return it->second;
                    }

                    const FVec3 pos  = positions[static_cast<size_t>(idx.V)];
                    const FVec3 norm = (idx.Vn >= 0 && static_cast<size_t>(idx.Vn) < normals.size())
                        ? normals[static_cast<size_t>(idx.Vn)]
                        : FVec3{};
                    const FVec2 uv = (idx.Vt >= 0 && static_cast<size_t>(idx.Vt) < texcoords.size())
                        ? texcoords[static_cast<size_t>(idx.Vt)]
                        : FVec2{};

                    outPositions.push_back(pos);
                    outNormals.push_back(norm);
                    outTexcoords.push_back(uv);

                    if (!boundsSet) {
                        outMesh.BoundsMin[0] = pos.X;
                        outMesh.BoundsMin[1] = pos.Y;
                        outMesh.BoundsMin[2] = pos.Z;
                        outMesh.BoundsMax[0] = pos.X;
                        outMesh.BoundsMax[1] = pos.Y;
                        outMesh.BoundsMax[2] = pos.Z;
                        boundsSet            = true;
                    } else {
                        outMesh.BoundsMin[0] = std::min(outMesh.BoundsMin[0], pos.X);
                        outMesh.BoundsMin[1] = std::min(outMesh.BoundsMin[1], pos.Y);
                        outMesh.BoundsMin[2] = std::min(outMesh.BoundsMin[2], pos.Z);
                        outMesh.BoundsMax[0] = std::max(outMesh.BoundsMax[0], pos.X);
                        outMesh.BoundsMax[1] = std::max(outMesh.BoundsMax[1], pos.Y);
                        outMesh.BoundsMax[2] = std::max(outMesh.BoundsMax[2], pos.Z);
                    }

                    const u32 newIndex = static_cast<u32>(outPositions.size() - 1);
                    indexMap.emplace(idx, newIndex);
                    return newIndex;
                };

                const FObjIndex& v0 = face[0];
                for (size_t i = 1; i + 1 < face.size(); ++i) {
                    const u32 i0 = emitVertex(v0);
                    const u32 i1 = emitVertex(face[i]);
                    const u32 i2 = emitVertex(face[i + 1]);
                    indices.push_back(i0);
                    indices.push_back(i1);
                    indices.push_back(i2);
                }
            }

            if (outPositions.empty() || indices.empty()) {
                return false;
            }

            const bool includeNormals   = hasNormal;
            const bool includeTexcoords = hasTexcoord;

            u32        offset = 0;
            outMesh.Attributes.clear();
            {
                Asset::FMeshVertexAttributeDesc attr{};
                attr.Semantic      = Asset::kMeshSemanticPosition;
                attr.Format        = Asset::kMeshVertexFormatR32G32B32Float;
                attr.AlignedOffset = offset;
                outMesh.Attributes.push_back(attr);
                offset += 12;
                outMesh.VertexFormatMask |= Asset::kMeshVertexMaskPosition;
            }
            if (includeNormals) {
                Asset::FMeshVertexAttributeDesc attr{};
                attr.Semantic      = Asset::kMeshSemanticNormal;
                attr.Format        = Asset::kMeshVertexFormatR32G32B32Float;
                attr.AlignedOffset = offset;
                outMesh.Attributes.push_back(attr);
                offset += 12;
                outMesh.VertexFormatMask |= Asset::kMeshVertexMaskNormal;
            }
            if (includeTexcoords) {
                Asset::FMeshVertexAttributeDesc attr{};
                attr.Semantic      = Asset::kMeshSemanticTexCoord;
                attr.SemanticIndex = 0;
                attr.Format        = Asset::kMeshVertexFormatR32G32Float;
                attr.AlignedOffset = offset;
                outMesh.Attributes.push_back(attr);
                offset += 8;
                outMesh.VertexFormatMask |= Asset::kMeshVertexMaskTexCoord0;
            }

            outMesh.VertexStride = offset;
            outMesh.VertexCount  = static_cast<u32>(outPositions.size());

            outMesh.VertexData.resize(
                static_cast<size_t>(outMesh.VertexStride) * outMesh.VertexCount);
            for (size_t i = 0; i < outPositions.size(); ++i) {
                u8* dst = outMesh.VertexData.data() + i * static_cast<size_t>(outMesh.VertexStride);
                std::memcpy(dst, &outPositions[i], sizeof(FVec3));
                u32 writeOffset = 12;
                if (includeNormals) {
                    std::memcpy(dst + writeOffset, &outNormals[i], sizeof(FVec3));
                    writeOffset += 12;
                }
                if (includeTexcoords) {
                    std::memcpy(dst + writeOffset, &outTexcoords[i], sizeof(FVec2));
                }
            }

            const u32 maxIndex =
                indices.empty() ? 0U : *std::max_element(indices.begin(), indices.end());
            outMesh.IndexType =
                (maxIndex <= 0xFFFFu) ? Asset::kMeshIndexTypeUint16 : Asset::kMeshIndexTypeUint32;
            outMesh.IndexCount = static_cast<u32>(indices.size());

            if (outMesh.IndexType == Asset::kMeshIndexTypeUint16) {
                outMesh.IndexData.resize(indices.size() * sizeof(u16));
                auto* dst = reinterpret_cast<u16*>(outMesh.IndexData.data());
                for (size_t i = 0; i < indices.size(); ++i) {
                    dst[i] = static_cast<u16>(indices[i]);
                }
            } else {
                outMesh.IndexData.resize(indices.size() * sizeof(u32));
                std::memcpy(outMesh.IndexData.data(), indices.data(), indices.size() * sizeof(u32));
            }

            Asset::FMeshSubMeshDesc subMesh{};
            subMesh.IndexStart   = 0;
            subMesh.IndexCount   = outMesh.IndexCount;
            subMesh.BaseVertex   = 0;
            subMesh.MaterialSlot = 0;
            outMesh.SubMeshes    = { subMesh };

            return true;
        }

        struct FGltfBufferView {
            u32 Buffer     = 0;
            u32 ByteOffset = 0;
            u32 ByteLength = 0;
            u32 ByteStride = 0;
        };

        struct FGltfAccessor {
            u32         BufferView    = 0;
            u32         ByteOffset    = 0;
            u32         Count         = 0;
            u32         ComponentType = 0;
            std::string Type;
        };

        auto ReadJsonU32(const FJsonValue* value, u32& out) -> bool {
            double number = 0.0;
            if (!GetNumberValue(value, number)) {
                return false;
            }
            if (number < 0.0 || number > static_cast<double>(std::numeric_limits<u32>::max())) {
                return false;
            }
            out = static_cast<u32>(number);
            return true;
        }

        auto ReadGltfBufferUri(const std::filesystem::path& basePath, const std::string& uri,
            std::vector<u8>& outBytes) -> bool {
            if (uri.rfind("data:", 0) == 0) {
                return false;
            }
            const std::filesystem::path bufferPath = basePath / uri;
            return ReadFileBytes(bufferPath, outBytes);
        }

        auto LoadGltfJson(const std::filesystem::path& sourcePath, std::string& outJson,
            std::vector<u8>& outBin) -> bool {
            const auto ext = sourcePath.extension().string();
            if (ext == ".glb") {
                std::vector<u8> bytes;
                if (!ReadFileBytes(sourcePath, bytes)) {
                    return false;
                }
                if (bytes.size() < 12) {
                    return false;
                }
                auto ReadU32 = [&](size_t offset) -> u32 {
                    u32 value = 0;
                    if (offset + sizeof(u32) > bytes.size()) {
                        return 0;
                    }
                    std::memcpy(&value, bytes.data() + offset, sizeof(u32));
                    return value;
                };
                if (ReadU32(0) != 0x46546C67u || ReadU32(4) != 2u) { // "glTF"
                    return false;
                }
                size_t offset = 12;
                outJson.clear();
                outBin.clear();
                while (offset + 8 <= bytes.size()) {
                    const u32 chunkLength = ReadU32(offset);
                    const u32 chunkType   = ReadU32(offset + 4);
                    offset += 8;
                    if (offset + chunkLength > bytes.size()) {
                        return false;
                    }
                    if (chunkType == 0x4E4F534Au) { // JSON
                        outJson.assign(reinterpret_cast<const char*>(bytes.data() + offset),
                            reinterpret_cast<const char*>(bytes.data() + offset + chunkLength));
                    } else if (chunkType == 0x004E4942u) { // BIN
                        outBin.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                            bytes.begin() + static_cast<std::ptrdiff_t>(offset + chunkLength));
                    }
                    offset += chunkLength;
                }
                return !outJson.empty();
            }

            return ReadFileText(sourcePath, outJson);
        }

        auto GetGltfArray(const FJsonValue& root, const char* key) -> const FJsonValue* {
            const FJsonValue* value = FindObjectValueInsensitive(root, key);
            if (value == nullptr || value->Type != EJsonType::Array) {
                return nullptr;
            }
            return value;
        }

        auto GetGltfObject(const FJsonValue& arrayValue, u32 index) -> const FJsonValue* {
            if (arrayValue.Type != EJsonType::Array) {
                return nullptr;
            }
            if (index >= arrayValue.Array.Size()) {
                return nullptr;
            }
            const FJsonValue* obj = arrayValue.Array[index];
            if (obj == nullptr || obj->Type != EJsonType::Object) {
                return nullptr;
            }
            return obj;
        }

        auto ReadAccessorFloats(const std::vector<std::vector<u8>>& buffers,
            const std::vector<FGltfBufferView>& views, const std::vector<FGltfAccessor>& accessors,
            u32 accessorIndex, u32 expectedComponents, std::vector<float>& outValues) -> bool {
            if (accessorIndex >= accessors.size()) {
                return false;
            }
            const auto& accessor = accessors[accessorIndex];
            if (accessor.ComponentType != 5126u) {
                return false;
            }
            if (accessor.Count == 0) {
                return false;
            }
            if (accessor.BufferView >= views.size()) {
                return false;
            }
            const auto& view = views[accessor.BufferView];
            if (view.Buffer >= buffers.size()) {
                return false;
            }
            const auto& buffer = buffers[view.Buffer];

            u32         components = 0;
            if (accessor.Type == "VEC2") {
                components = 2;
            } else if (accessor.Type == "VEC3") {
                components = 3;
            } else if (accessor.Type == "VEC4") {
                components = 4;
            } else if (accessor.Type == "SCALAR") {
                components = 1;
            }

            if (components != expectedComponents) {
                return false;
            }

            const u32 componentSize = 4;
            const u32 stride =
                (view.ByteStride != 0) ? view.ByteStride : (components * componentSize);
            const u64 baseOffset = static_cast<u64>(view.ByteOffset) + accessor.ByteOffset;
            const u64 required   = static_cast<u64>(stride) * (accessor.Count - 1)
                + static_cast<u64>(components * componentSize);
            if (baseOffset + required > buffer.size()) {
                return false;
            }

            outValues.resize(static_cast<size_t>(accessor.Count) * components);
            for (u32 i = 0; i < accessor.Count; ++i) {
                const u64 srcOffset = baseOffset + static_cast<u64>(i) * stride;
                std::memcpy(outValues.data() + static_cast<size_t>(i) * components,
                    buffer.data() + static_cast<size_t>(srcOffset), components * componentSize);
            }
            return true;
        }

        auto ReadAccessorIndices(const std::vector<std::vector<u8>>& buffers,
            const std::vector<FGltfBufferView>& views, const std::vector<FGltfAccessor>& accessors,
            u32 accessorIndex, std::vector<u32>& outIndices) -> bool {
            if (accessorIndex >= accessors.size()) {
                return false;
            }
            const auto& accessor = accessors[accessorIndex];
            if (accessor.Type != "SCALAR") {
                return false;
            }
            if (accessor.Count == 0) {
                return false;
            }
            if (accessor.BufferView >= views.size()) {
                return false;
            }
            const auto& view = views[accessor.BufferView];
            if (view.Buffer >= buffers.size()) {
                return false;
            }
            const auto& buffer = buffers[view.Buffer];

            u32         componentSize = 0;
            if (accessor.ComponentType == 5123u) {
                componentSize = 2;
            } else if (accessor.ComponentType == 5125u) {
                componentSize = 4;
            } else {
                return false;
            }

            const u32 stride     = (view.ByteStride != 0) ? view.ByteStride : componentSize;
            const u64 baseOffset = static_cast<u64>(view.ByteOffset) + accessor.ByteOffset;
            const u64 required =
                static_cast<u64>(stride) * (accessor.Count - 1) + static_cast<u64>(componentSize);
            if (baseOffset + required > buffer.size()) {
                return false;
            }

            outIndices.resize(accessor.Count);
            for (u32 i = 0; i < accessor.Count; ++i) {
                const u64 srcOffset = baseOffset + static_cast<u64>(i) * stride;
                if (componentSize == 2) {
                    u16 value = 0;
                    std::memcpy(
                        &value, buffer.data() + static_cast<size_t>(srcOffset), sizeof(u16));
                    outIndices[i] = value;
                } else {
                    u32 value = 0;
                    std::memcpy(
                        &value, buffer.data() + static_cast<size_t>(srcOffset), sizeof(u32));
                    outIndices[i] = value;
                }
            }
            return true;
        }

        auto CookMeshFromGltf(const std::filesystem::path& sourcePath, FMeshBuildResult& outMesh,
            std::vector<u8>& outCookKeyBytes) -> bool {
            std::string     jsonText;
            std::vector<u8> binChunk;
            if (!LoadGltfJson(sourcePath, jsonText, binChunk)) {
                return false;
            }

            FNativeString native;
            native.Append(jsonText.c_str(), jsonText.size());
            const FNativeStringView view(native.GetData(), native.Length());

            FJsonDocument           document;
            if (!document.Parse(view)) {
                return false;
            }

            const FJsonValue* root = document.GetRoot();
            if (root == nullptr || root->Type != EJsonType::Object) {
                return false;
            }

            const auto* buffersValue = GetGltfArray(*root, "buffers");
            if (buffersValue == nullptr) {
                return false;
            }

            std::vector<std::vector<u8>> buffers;
            buffers.reserve(buffersValue->Array.Size());

            const std::filesystem::path basePath = sourcePath.parent_path();
            for (u32 i = 0; i < buffersValue->Array.Size(); ++i) {
                const FJsonValue* bufferObj = GetGltfObject(*buffersValue, i);
                if (bufferObj == nullptr) {
                    return false;
                }

                std::vector<u8> bufferBytes;
                FNativeString   uriText;
                if (GetStringValue(FindObjectValueInsensitive(*bufferObj, "Uri"), uriText)) {
                    const std::string uri = ToStdString(uriText);
                    if (!ReadGltfBufferUri(basePath, uri, bufferBytes)) {
                        return false;
                    }
                } else {
                    if (i != 0 || binChunk.empty()) {
                        return false;
                    }
                    bufferBytes = binChunk;
                }

                outCookKeyBytes.insert(
                    outCookKeyBytes.end(), bufferBytes.begin(), bufferBytes.end());
                buffers.push_back(AltinaEngine::Move(bufferBytes));
            }

            const auto* bufferViewsValue = GetGltfArray(*root, "bufferViews");
            if (bufferViewsValue == nullptr) {
                return false;
            }

            std::vector<FGltfBufferView> bufferViews;
            bufferViews.reserve(bufferViewsValue->Array.Size());
            for (u32 i = 0; i < bufferViewsValue->Array.Size(); ++i) {
                const FJsonValue* viewObj = GetGltfObject(*bufferViewsValue, i);
                if (viewObj == nullptr) {
                    return false;
                }

                FGltfBufferView bufferView{};
                if (!ReadJsonU32(FindObjectValueInsensitive(*viewObj, "Buffer"), bufferView.Buffer)
                    || !ReadJsonU32(FindObjectValueInsensitive(*viewObj, "ByteLength"),
                        bufferView.ByteLength)) {
                    return false;
                }
                ReadJsonU32(
                    FindObjectValueInsensitive(*viewObj, "ByteOffset"), bufferView.ByteOffset);
                ReadJsonU32(
                    FindObjectValueInsensitive(*viewObj, "ByteStride"), bufferView.ByteStride);
                bufferViews.push_back(bufferView);
            }

            const auto* accessorsValue = GetGltfArray(*root, "accessors");
            if (accessorsValue == nullptr) {
                return false;
            }

            std::vector<FGltfAccessor> accessors;
            accessors.reserve(accessorsValue->Array.Size());
            for (u32 i = 0; i < accessorsValue->Array.Size(); ++i) {
                const FJsonValue* accessorObj = GetGltfObject(*accessorsValue, i);
                if (accessorObj == nullptr) {
                    return false;
                }

                FGltfAccessor accessor{};
                if (!ReadJsonU32(
                        FindObjectValueInsensitive(*accessorObj, "BufferView"), accessor.BufferView)
                    || !ReadJsonU32(FindObjectValueInsensitive(*accessorObj, "ComponentType"),
                        accessor.ComponentType)
                    || !ReadJsonU32(
                        FindObjectValueInsensitive(*accessorObj, "Count"), accessor.Count)) {
                    return false;
                }
                ReadJsonU32(
                    FindObjectValueInsensitive(*accessorObj, "ByteOffset"), accessor.ByteOffset);

                FNativeString typeText;
                if (!GetStringValue(FindObjectValueInsensitive(*accessorObj, "Type"), typeText)) {
                    return false;
                }
                accessor.Type = ToStdString(typeText);
                accessors.push_back(accessor);
            }

            const auto* meshesValue = GetGltfArray(*root, "meshes");
            if (meshesValue == nullptr || meshesValue->Array.IsEmpty()) {
                return false;
            }
            const FJsonValue* meshObj = GetGltfObject(*meshesValue, 0);
            if (meshObj == nullptr) {
                return false;
            }

            const auto* primsValue = GetGltfArray(*meshObj, "primitives");
            if (primsValue == nullptr || primsValue->Array.IsEmpty()) {
                return false;
            }
            const FJsonValue* primObj = GetGltfObject(*primsValue, 0);
            if (primObj == nullptr) {
                return false;
            }

            u32 mode = 4;
            ReadJsonU32(FindObjectValueInsensitive(*primObj, "Mode"), mode);
            if (mode != 4) {
                return false;
            }

            const FJsonValue* attrsObj = FindObjectValueInsensitive(*primObj, "Attributes");
            if (attrsObj == nullptr || attrsObj->Type != EJsonType::Object) {
                return false;
            }

            u32 positionAccessor = 0;
            if (!ReadJsonU32(FindObjectValueInsensitive(*attrsObj, "POSITION"), positionAccessor)) {
                return false;
            }

            u32 normalAccessor = std::numeric_limits<u32>::max();
            ReadJsonU32(FindObjectValueInsensitive(*attrsObj, "NORMAL"), normalAccessor);
            u32 uvAccessor = std::numeric_limits<u32>::max();
            ReadJsonU32(FindObjectValueInsensitive(*attrsObj, "TEXCOORD_0"), uvAccessor);

            std::vector<float> positions;
            std::vector<float> normals;
            std::vector<float> uvs;
            if (!ReadAccessorFloats(
                    buffers, bufferViews, accessors, positionAccessor, 3, positions)) {
                return false;
            }
            if (normalAccessor != std::numeric_limits<u32>::max()) {
                if (!ReadAccessorFloats(
                        buffers, bufferViews, accessors, normalAccessor, 3, normals)) {
                    return false;
                }
            }
            if (uvAccessor != std::numeric_limits<u32>::max()) {
                if (!ReadAccessorFloats(buffers, bufferViews, accessors, uvAccessor, 2, uvs)) {
                    return false;
                }
            }

            const u32 vertexCount = static_cast<u32>(positions.size() / 3);
            if (vertexCount == 0) {
                return false;
            }
            if (!normals.empty() && normals.size() / 3 != vertexCount) {
                return false;
            }
            if (!uvs.empty() && uvs.size() / 2 != vertexCount) {
                return false;
            }

            std::vector<u32> indices;
            u32              indicesAccessor = std::numeric_limits<u32>::max();
            ReadJsonU32(FindObjectValueInsensitive(*primObj, "Indices"), indicesAccessor);
            if (indicesAccessor != std::numeric_limits<u32>::max()) {
                if (!ReadAccessorIndices(
                        buffers, bufferViews, accessors, indicesAccessor, indices)) {
                    return false;
                }
            } else {
                if (vertexCount % 3 != 0) {
                    return false;
                }
                indices.resize(vertexCount);
                for (u32 i = 0; i < vertexCount; ++i) {
                    indices[i] = i;
                }
            }

            if (indices.empty() || (indices.size() % 3 != 0)) {
                return false;
            }

            const bool includeNormals   = !normals.empty();
            const bool includeTexcoords = !uvs.empty();

            u32        offset = 0;
            outMesh.Attributes.clear();
            {
                Asset::FMeshVertexAttributeDesc attr{};
                attr.Semantic      = Asset::kMeshSemanticPosition;
                attr.Format        = Asset::kMeshVertexFormatR32G32B32Float;
                attr.AlignedOffset = offset;
                outMesh.Attributes.push_back(attr);
                offset += 12;
                outMesh.VertexFormatMask |= Asset::kMeshVertexMaskPosition;
            }
            if (includeNormals) {
                Asset::FMeshVertexAttributeDesc attr{};
                attr.Semantic      = Asset::kMeshSemanticNormal;
                attr.Format        = Asset::kMeshVertexFormatR32G32B32Float;
                attr.AlignedOffset = offset;
                outMesh.Attributes.push_back(attr);
                offset += 12;
                outMesh.VertexFormatMask |= Asset::kMeshVertexMaskNormal;
            }
            if (includeTexcoords) {
                Asset::FMeshVertexAttributeDesc attr{};
                attr.Semantic      = Asset::kMeshSemanticTexCoord;
                attr.SemanticIndex = 0;
                attr.Format        = Asset::kMeshVertexFormatR32G32Float;
                attr.AlignedOffset = offset;
                outMesh.Attributes.push_back(attr);
                offset += 8;
                outMesh.VertexFormatMask |= Asset::kMeshVertexMaskTexCoord0;
            }

            outMesh.VertexStride = offset;
            outMesh.VertexCount  = vertexCount;

            outMesh.VertexData.resize(static_cast<size_t>(outMesh.VertexStride) * vertexCount);
            bool boundsSet = false;
            for (u32 i = 0; i < vertexCount; ++i) {
                u8* dst = outMesh.VertexData.data()
                    + static_cast<size_t>(i) * static_cast<size_t>(outMesh.VertexStride);
                const FVec3 pos{ positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2] };
                std::memcpy(dst, &pos, sizeof(FVec3));
                u32 writeOffset = 12;
                if (includeNormals) {
                    const FVec3 n{ normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2] };
                    std::memcpy(dst + writeOffset, &n, sizeof(FVec3));
                    writeOffset += 12;
                }
                if (includeTexcoords) {
                    const FVec2 uv{ uvs[i * 2], uvs[i * 2 + 1] };
                    std::memcpy(dst + writeOffset, &uv, sizeof(FVec2));
                }

                if (!boundsSet) {
                    outMesh.BoundsMin[0] = pos.X;
                    outMesh.BoundsMin[1] = pos.Y;
                    outMesh.BoundsMin[2] = pos.Z;
                    outMesh.BoundsMax[0] = pos.X;
                    outMesh.BoundsMax[1] = pos.Y;
                    outMesh.BoundsMax[2] = pos.Z;
                    boundsSet            = true;
                } else {
                    outMesh.BoundsMin[0] = std::min(outMesh.BoundsMin[0], pos.X);
                    outMesh.BoundsMin[1] = std::min(outMesh.BoundsMin[1], pos.Y);
                    outMesh.BoundsMin[2] = std::min(outMesh.BoundsMin[2], pos.Z);
                    outMesh.BoundsMax[0] = std::max(outMesh.BoundsMax[0], pos.X);
                    outMesh.BoundsMax[1] = std::max(outMesh.BoundsMax[1], pos.Y);
                    outMesh.BoundsMax[2] = std::max(outMesh.BoundsMax[2], pos.Z);
                }
            }

            u32 maxIndex = 0;
            for (u32 idx : indices) {
                maxIndex = std::max(maxIndex, idx);
            }

            outMesh.IndexType =
                (maxIndex <= 0xFFFFu) ? Asset::kMeshIndexTypeUint16 : Asset::kMeshIndexTypeUint32;
            outMesh.IndexCount = static_cast<u32>(indices.size());

            if (outMesh.IndexType == Asset::kMeshIndexTypeUint16) {
                outMesh.IndexData.resize(indices.size() * sizeof(u16));
                auto* dst = reinterpret_cast<u16*>(outMesh.IndexData.data());
                for (size_t i = 0; i < indices.size(); ++i) {
                    dst[i] = static_cast<u16>(indices[i]);
                }
            } else {
                outMesh.IndexData.resize(indices.size() * sizeof(u32));
                std::memcpy(outMesh.IndexData.data(), indices.data(), indices.size() * sizeof(u32));
            }

            Asset::FMeshSubMeshDesc subMesh{};
            subMesh.IndexStart   = 0;
            subMesh.IndexCount   = outMesh.IndexCount;
            subMesh.BaseVertex   = 0;
            subMesh.MaterialSlot = 0;
            outMesh.SubMeshes    = { subMesh };

            return true;
        }

        auto CookMesh(const std::filesystem::path& sourcePath, std::vector<u8>& outCooked,
            Asset::FMeshDesc& outDesc, std::vector<u8>& outCookKeyBytes) -> bool {
            const std::string ext      = sourcePath.extension().string();
            std::string       extLower = ext;
            ToLowerAscii(extLower);

            FMeshBuildResult mesh{};
            bool             ok = false;
            if (extLower == ".obj") {
                ok = CookMeshFromObj(sourcePath, mesh);
            } else if (extLower == ".gltf" || extLower == ".glb") {
                ok = CookMeshFromGltf(sourcePath, mesh, outCookKeyBytes);
            } else {
                return false;
            }

            if (!ok) {
                return false;
            }

            return BuildMeshBlob(mesh, outCooked, outDesc);
        }

        struct FBundledAsset {
            FUuid             Uuid;
            std::string       UuidText;
            Asset::EAssetType Type = Asset::EAssetType::Unknown;
            std::string       CookedPath;
            std::vector<u8>   Data;
        };

        void WriteBundleUuid(Asset::FBundleIndexEntry& entry, const FUuid& uuid) {
            const auto& bytes = uuid.GetBytes();
            for (usize index = 0; index < FUuid::kByteCount; ++index) {
                entry.Uuid[index] = bytes[index];
            }
        }

        auto LoadRegistryAssets(const std::filesystem::path& registryPath,
            const std::filesystem::path& cookedRoot, std::vector<FBundledAsset>& outAssets)
            -> bool {
            outAssets.clear();

            std::string text;
            if (!ReadFileText(registryPath, text)) {
                return false;
            }

            FNativeString native;
            native.Append(text.c_str(), text.size());
            const FNativeStringView view(native.GetData(), native.Length());

            FJsonDocument           document;
            if (!document.Parse(view)) {
                return false;
            }

            const FJsonValue* root = document.GetRoot();
            if (root == nullptr || root->Type != EJsonType::Object) {
                return false;
            }

            const FJsonValue* assetsValue = FindObjectValueInsensitive(*root, "Assets");
            if (assetsValue == nullptr || assetsValue->Type != EJsonType::Array) {
                return false;
            }

            for (const auto* assetValue : assetsValue->Array) {
                if (assetValue == nullptr || assetValue->Type != EJsonType::Object) {
                    continue;
                }

                FNativeString uuidText;
                FNativeString typeText;
                FNativeString cookedText;

                if (!GetStringValue(FindObjectValueInsensitive(*assetValue, "Uuid"), uuidText)) {
                    continue;
                }
                if (!GetStringValue(FindObjectValueInsensitive(*assetValue, "Type"), typeText)) {
                    continue;
                }
                if (!GetStringValue(
                        FindObjectValueInsensitive(*assetValue, "CookedPath"), cookedText)) {
                    continue;
                }

                FUuid uuid;
                if (!FUuid::TryParse(
                        FNativeStringView(uuidText.GetData(), uuidText.Length()), uuid)) {
                    continue;
                }

                FBundledAsset asset{};
                asset.Uuid     = uuid;
                asset.UuidText = ToStdString(uuidText);
                asset.Type     = ParseAssetType(ToStdString(typeText));
                if (asset.Type == Asset::EAssetType::Unknown) {
                    continue;
                }
                asset.CookedPath = ToStdString(cookedText);

                const std::filesystem::path sourcePath = cookedRoot / asset.CookedPath;
                if (!ReadFileBytes(sourcePath, asset.Data)) {
                    std::cerr << "Failed to read cooked asset: " << sourcePath.string() << "\n";
                    continue;
                }

                outAssets.push_back(AltinaEngine::Move(asset));
            }

            return !outAssets.empty();
        }
        auto LoadMeta(const std::filesystem::path& metaPath, FUuid& outUuid,
            Asset::EAssetType& outType, std::string& outVirtualPath) -> bool {
            std::string text;
            if (!ReadFileText(metaPath, text)) {
                return false;
            }

            FNativeString native;
            native.Append(text.c_str(), text.size());
            const FNativeStringView view(native.GetData(), native.Length());

            FJsonDocument           document;
            if (!document.Parse(view)) {
                return false;
            }

            const FJsonValue* root = document.GetRoot();
            if (root == nullptr || root->Type != EJsonType::Object) {
                return false;
            }

            FNativeString uuidText;
            if (!GetStringValue(FindObjectValueInsensitive(*root, "Uuid"), uuidText)) {
                return false;
            }

            const FNativeStringView uuidView(uuidText.GetData(), uuidText.Length());
            if (!FUuid::TryParse(uuidView, outUuid)) {
                return false;
            }

            FNativeString typeText;
            if (GetStringValue(FindObjectValueInsensitive(*root, "Type"), typeText)) {
                outType = ParseAssetType(ToStdString(typeText));
            }

            FNativeString pathText;
            if (GetStringValue(FindObjectValueInsensitive(*root, "VirtualPath"), pathText)) {
                outVirtualPath = ToStdString(pathText);
                ToLowerAscii(outVirtualPath);
            }

            return true;
        }

        auto WriteMetaFile(const FAssetRecord& asset) -> bool {
            const std::string  uuid = ToStdString(asset.Uuid.ToNativeString());

            std::ostringstream stream;
            stream << "{\n";
            stream << "  \"Uuid\": \"" << EscapeJson(uuid) << "\",\n";
            stream << "  \"Type\": \"" << AssetTypeToString(asset.Type) << "\",\n";
            stream << "  \"VirtualPath\": \"" << EscapeJson(asset.VirtualPath) << "\",\n";
            stream << "  \"SourcePath\": \"" << EscapeJson(asset.SourcePathRel) << "\",\n";
            stream << "  \"Importer\": \"" << EscapeJson(asset.ImporterName) << "\",\n";
            stream << "  \"ImporterVersion\": " << asset.ImporterVersion << ",\n";
            stream << "  \"Dependencies\": []\n";
            stream << "}\n";

            return WriteTextFile(asset.MetaPath, stream.str());
        }
        void CollectAssetsInDirectory(const std::filesystem::path& assetsRoot,
            const std::string& virtualPrefix, const std::filesystem::path& repoRoot,
            std::vector<FAssetRecord>& outAssets) {
            std::error_code ec;
            if (!std::filesystem::exists(assetsRoot, ec)) {
                return;
            }

            std::filesystem::recursive_directory_iterator it(
                assetsRoot, std::filesystem::directory_options::skip_permission_denied, ec);
            const std::filesystem::recursive_directory_iterator end;
            for (; it != end; it.increment(ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }

                if (!it->is_regular_file(ec)) {
                    ec.clear();
                    continue;
                }

                const std::filesystem::path sourcePath = it->path();
                if (sourcePath.extension() == ".meta") {
                    continue;
                }

                const Asset::EAssetType type = GuessAssetType(sourcePath);
                if (type == Asset::EAssetType::Unknown) {
                    continue;
                }

                std::string           sourceRel = MakeRelativePath(repoRoot, sourcePath);

                std::filesystem::path relVirtual =
                    std::filesystem::relative(sourcePath, assetsRoot, ec);
                if (ec) {
                    relVirtual = sourcePath.filename();
                    ec.clear();
                }
                relVirtual.replace_extension();

                std::string virtualPath = virtualPrefix;
                if (!virtualPath.empty() && virtualPath.back() != '/') {
                    virtualPath.push_back('/');
                }
                virtualPath += NormalizePath(relVirtual);
                ToLowerAscii(virtualPath);

                FAssetRecord record{};
                record.SourcePath = sourcePath;
                record.MetaPath   = sourcePath;
                record.MetaPath += ".meta";
                record.SourcePathRel   = sourceRel;
                record.VirtualPath     = virtualPath;
                record.Type            = type;
                record.ImporterName    = GetImporterName(type);
                record.ImporterVersion = 1U;

                outAssets.push_back(record);
            }
        }
        auto CollectAssets(const std::filesystem::path& repoRoot, const std::string& demoFilter,
            std::vector<FAssetRecord>& outAssets) -> bool {
            outAssets.clear();

            CollectAssetsInDirectory(repoRoot / "Assets", "Engine", repoRoot, outAssets);

            std::error_code             ec;
            const std::filesystem::path demoRoot = repoRoot / "Demo";
            if (!std::filesystem::exists(demoRoot, ec)) {
                return true;
            }

            for (const auto& entry : std::filesystem::directory_iterator(demoRoot, ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }

                if (!entry.is_directory(ec)) {
                    ec.clear();
                    continue;
                }

                const std::string demoName = entry.path().filename().string();
                if (!demoFilter.empty() && demoName != demoFilter) {
                    continue;
                }

                const std::filesystem::path demoAssets = entry.path() / "Assets";
                CollectAssetsInDirectory(demoAssets, "Demo/" + demoName, repoRoot, outAssets);
            }

            return true;
        }

        auto EnsureMeta(FAssetRecord& asset, EMetaWriteMode mode) -> bool {
            Asset::EAssetType metaType = Asset::EAssetType::Unknown;
            std::string       metaVirtualPath;
            if (LoadMeta(asset.MetaPath, asset.Uuid, metaType, metaVirtualPath)) {
                if (metaType != Asset::EAssetType::Unknown) {
                    asset.Type = metaType;
                }
                if (!metaVirtualPath.empty()) {
                    asset.VirtualPath = metaVirtualPath;
                }
                asset.ImporterName = GetImporterName(asset.Type);

                if (mode == EMetaWriteMode::MissingOnly) {
                    return true;
                }
                return WriteMetaFile(asset);
            }

            asset.Uuid         = FUuid::New();
            asset.ImporterName = GetImporterName(asset.Type);
            return WriteMetaFile(asset);
        }
        auto ParseCommandLine(
            int argc, char** argv, FCommandLine& outCommand, std::string& outError) -> bool {
            if (argc < 2) {
                outError = "Missing command.";
                return false;
            }

            outCommand.Command = argv[1];
            for (int index = 2; index < argc; ++index) {
                std::string arg = argv[index];
                if (arg.rfind("--", 0) == 0) {
                    std::string key   = arg.substr(2);
                    std::string value = "true";
                    if (index + 1 < argc && std::string(argv[index + 1]).rfind("--", 0) != 0) {
                        value = argv[index + 1];
                        ++index;
                    }
                    outCommand.Options[key] = value;
                }
            }

            return true;
        }

        void PrintUsage() {
            std::cout << "AssetTool commands:\n";
            std::cout << "  import   --root <repoRoot> [--demo <DemoName>]\n";
            std::cout << "  cook     --root <repoRoot> --platform <Platform> [--demo <DemoName>]";
            std::cout << " [--build-root <BuildRoot>]\n";
            std::cout << "  bundle   --root <repoRoot> --platform <Platform> [--demo <DemoName>]";
            std::cout << " [--build-root <BuildRoot>]\n";
            std::cout << "  validate --registry <PathToAssetRegistry.json>\n";
            std::cout << "  clean    --root <repoRoot> [--build-root <BuildRoot>] --cache\n";
        }

        auto BuildPaths(const FCommandLine& command, const std::string& platform) -> FToolPaths {
            std::filesystem::path root;
            auto                  rootIt = command.Options.find("root");
            if (rootIt != command.Options.end()) {
                root = std::filesystem::path(rootIt->second);
            } else {
                root = std::filesystem::current_path();
            }

            std::filesystem::path buildRoot = root / "build";
            auto                  buildIt   = command.Options.find("build-root");
            if (buildIt != command.Options.end()) {
                buildRoot = std::filesystem::path(buildIt->second);
            }

            FToolPaths paths{};
            paths.Root          = std::filesystem::absolute(root);
            paths.BuildRoot     = std::filesystem::absolute(buildRoot);
            paths.CookedRoot    = paths.BuildRoot / "Cooked" / platform;
            paths.CacheRoot     = paths.BuildRoot / "Cache";
            paths.CookCachePath = paths.CacheRoot / "CookKeys.json";
            return paths;
        }
        auto LoadCookCache(const std::filesystem::path&       cachePath,
            std::unordered_map<std::string, FCookCacheEntry>& outEntries) -> bool {
            outEntries.clear();
            if (!std::filesystem::exists(cachePath)) {
                return true;
            }

            std::string text;
            if (!ReadFileText(cachePath, text)) {
                return false;
            }

            FNativeString native;
            native.Append(text.c_str(), text.size());
            const FNativeStringView view(native.GetData(), native.Length());

            FJsonDocument           document;
            if (!document.Parse(view)) {
                return false;
            }

            const FJsonValue* root = document.GetRoot();
            if (root == nullptr || root->Type != EJsonType::Object) {
                return false;
            }

            const FJsonValue* entriesValue = FindObjectValueInsensitive(*root, "Entries");
            if (entriesValue == nullptr || entriesValue->Type != EJsonType::Array) {
                return true;
            }

            for (const auto* entry : entriesValue->Array) {
                if (entry == nullptr || entry->Type != EJsonType::Object) {
                    continue;
                }

                FNativeString uuidText;
                FNativeString cookKeyText;
                if (!GetStringValue(FindObjectValueInsensitive(*entry, "Uuid"), uuidText)
                    || !GetStringValue(
                        FindObjectValueInsensitive(*entry, "CookKey"), cookKeyText)) {
                    continue;
                }

                FCookCacheEntry cacheEntry{};
                cacheEntry.Uuid    = ToStdString(uuidText);
                cacheEntry.CookKey = ToStdString(cookKeyText);

                FNativeString sourceText;
                if (GetStringValue(FindObjectValueInsensitive(*entry, "SourcePath"), sourceText)) {
                    cacheEntry.SourcePath = ToStdString(sourceText);
                }

                FNativeString cookedText;
                if (GetStringValue(FindObjectValueInsensitive(*entry, "CookedPath"), cookedText)) {
                    cacheEntry.CookedPath = ToStdString(cookedText);
                }

                FNativeString lastCookedText;
                if (GetStringValue(
                        FindObjectValueInsensitive(*entry, "LastCooked"), lastCookedText)) {
                    cacheEntry.LastCooked = ToStdString(lastCookedText);
                }

                if (!cacheEntry.Uuid.empty()) {
                    outEntries[cacheEntry.Uuid] = cacheEntry;
                }
            }

            return true;
        }
        auto SaveCookCache(const std::filesystem::path&             cachePath,
            const std::unordered_map<std::string, FCookCacheEntry>& entries) -> bool {
            std::vector<std::string> keys;
            keys.reserve(entries.size());
            for (const auto& pair : entries) {
                keys.push_back(pair.first);
            }
            std::sort(keys.begin(), keys.end());

            std::ostringstream stream;
            stream << "{\n";
            stream << "  \"Version\": 1,\n";
            stream << "  \"Entries\": [\n";

            bool first = true;
            for (const auto& key : keys) {
                const auto& entry = entries.at(key);
                if (!first) {
                    stream << ",\n";
                }
                first = false;

                stream << "    {\n";
                stream << "      \"Uuid\": \"" << EscapeJson(entry.Uuid) << "\",\n";
                stream << "      \"CookKey\": \"" << EscapeJson(entry.CookKey) << "\",\n";
                stream << "      \"SourcePath\": \"" << EscapeJson(entry.SourcePath) << "\",\n";
                stream << "      \"CookedPath\": \"" << EscapeJson(entry.CookedPath) << "\"";
                if (!entry.LastCooked.empty()) {
                    stream << ",\n";
                    stream << "      \"LastCooked\": \"" << EscapeJson(entry.LastCooked) << "\"\n";
                } else {
                    stream << "\n";
                }
                stream << "    }";
            }

            stream << "\n  ]\n";
            stream << "}\n";

            std::error_code ec;
            std::filesystem::create_directories(cachePath.parent_path(), ec);

            return WriteTextFile(cachePath, stream.str());
        }
        auto WriteRegistry(const std::filesystem::path& registryPath,
            const std::vector<FRegistryEntry>&          assets) -> bool {
            std::ostringstream stream;
            stream << "{\n";
            stream << "  \"SchemaVersion\": 1,\n";
            stream << "  \"Assets\": [\n";

            for (size_t index = 0; index < assets.size(); ++index) {
                const auto& entry = assets[index];
                stream << "    {\n";
                stream << "      \"Uuid\": \"" << EscapeJson(entry.Uuid) << "\",\n";
                stream << "      \"Type\": \"" << AssetTypeToString(entry.Type) << "\",\n";
                stream << "      \"VirtualPath\": \"" << EscapeJson(entry.VirtualPath) << "\",\n";
                stream << "      \"CookedPath\": \"" << EscapeJson(entry.CookedPath) << "\",\n";
                stream << "      \"Dependencies\": [],\n";
                stream << "      \"Desc\": {";

                switch (entry.Type) {
                    case Asset::EAssetType::Texture2D:
                        if (entry.HasTextureDesc) {
                            stream << "\"Width\": " << entry.TextureDesc.Width
                                   << ", \"Height\": " << entry.TextureDesc.Height
                                   << ", \"Format\": " << entry.TextureDesc.Format
                                   << ", \"MipCount\": " << entry.TextureDesc.MipCount
                                   << ", \"SRGB\": " << (entry.TextureDesc.SRGB ? "true" : "false");
                        } else {
                            stream
                                << "\"Width\": 0, \"Height\": 0, \"Format\": 0, \"MipCount\": 0, \"SRGB\": true";
                        }
                        break;
                    case Asset::EAssetType::Mesh:
                        if (entry.HasMeshDesc) {
                            stream << "\"VertexFormat\": " << entry.MeshDesc.VertexFormat
                                   << ", \"IndexFormat\": " << entry.MeshDesc.IndexFormat
                                   << ", \"SubMeshCount\": " << entry.MeshDesc.SubMeshCount;
                        } else {
                            stream
                                << "\"VertexFormat\": 0, \"IndexFormat\": 0, \"SubMeshCount\": 0";
                        }
                        break;
                    case Asset::EAssetType::Material:
                        stream << "\"ShadingModel\": 0, \"BlendMode\": 0, \"Flags\": 0, "
                               << "\"AlphaCutoff\": 0, \"TextureBindings\": []";
                        break;
                    case Asset::EAssetType::Audio:
                        if (entry.HasAudioDesc) {
                            stream << "\"Codec\": " << entry.AudioDesc.Codec
                                   << ", \"Channels\": " << entry.AudioDesc.Channels
                                   << ", \"SampleRate\": " << entry.AudioDesc.SampleRate
                                   << ", \"Duration\": " << entry.AudioDesc.DurationSeconds;
                        } else {
                            stream
                                << "\"Codec\": 0, \"Channels\": 0, \"SampleRate\": 0, \"Duration\": 0";
                        }
                        break;
                    default:
                        break;
                }

                stream << "}\n";
                stream << "    }";
                if (index + 1 < assets.size()) {
                    stream << ",";
                }
                stream << "\n";
            }

            stream << "  ],\n";
            stream << "  \"Redirectors\": []\n";
            stream << "}\n";

            std::error_code ec;
            std::filesystem::create_directories(registryPath.parent_path(), ec);

            return WriteTextFile(registryPath, stream.str());
        }
        auto ImportAssets(const FCommandLine& command) -> int {
            const std::string demoFilter =
                command.Options.contains("demo") ? command.Options.at("demo") : std::string();

            FToolPaths                paths = BuildPaths(command, "Win64");

            std::vector<FAssetRecord> assets;
            if (!CollectAssets(paths.Root, demoFilter, assets)) {
                std::cerr << "Failed to collect assets.\n";
                return 1;
            }

            size_t written = 0U;
            for (auto& asset : assets) {
                if (!EnsureMeta(asset, EMetaWriteMode::Always)) {
                    std::cerr << "Failed to write meta: " << asset.MetaPath.string() << "\n";
                    continue;
                }
                ++written;
            }

            std::cout << "Imported assets: " << written << "\n";
            return 0;
        }
        auto CookAssets(const FCommandLine& command) -> int {
            const std::string platform = command.Options.contains("platform")
                ? command.Options.at("platform")
                : std::string("Win64");
            const std::string demoFilter =
                command.Options.contains("demo") ? command.Options.at("demo") : std::string();

            FToolPaths                paths = BuildPaths(command, platform);

            std::vector<FAssetRecord> assets;
            if (!CollectAssets(paths.Root, demoFilter, assets)) {
                std::cerr << "Failed to collect assets.\n";
                return 1;
            }

            std::unordered_map<std::string, FCookCacheEntry> cacheEntries;
            if (!LoadCookCache(paths.CookCachePath, cacheEntries)) {
                std::cerr << "Failed to read cook cache: " << paths.CookCachePath.string() << "\n";
                return 1;
            }

            std::vector<FRegistryEntry> registryAssets;
            registryAssets.reserve(assets.size());

            size_t cookedCount = 0U;
            for (auto& asset : assets) {
                if (!EnsureMeta(asset, EMetaWriteMode::MissingOnly)) {
                    std::cerr << "Failed to ensure meta: " << asset.MetaPath.string() << "\n";
                    continue;
                }

                std::vector<u8> bytes;
                if (!ReadFileBytes(asset.SourcePath, bytes)) {
                    std::cerr << "Failed to read source: " << asset.SourcePath.string() << "\n";
                    continue;
                }

                std::vector<u8>       cookedBytes;
                std::vector<u8>       cookKeyExtras;
                Asset::FTexture2DDesc textureDesc{};
                Asset::FMeshDesc      meshDesc{};
                Asset::FAudioDesc     audioDesc{};
                const bool            isTexture = asset.Type == Asset::EAssetType::Texture2D;
                const bool            isMesh    = asset.Type == Asset::EAssetType::Mesh;
                const bool            isAudio   = asset.Type == Asset::EAssetType::Audio;
                if (isTexture) {
                    constexpr bool kDefaultSrgb = true;
                    if (!CookTexture2D(bytes, kDefaultSrgb, cookedBytes, textureDesc)) {
                        std::cerr << "Failed to cook texture: " << asset.SourcePath.string()
                                  << "\n";
                        continue;
                    }
                } else if (isMesh) {
                    if (!CookMesh(asset.SourcePath, cookedBytes, meshDesc, cookKeyExtras)) {
                        std::cerr << "Failed to cook mesh: " << asset.SourcePath.string() << "\n";
                        continue;
                    }
                } else if (isAudio) {
                    if (!CookAudio(asset.SourcePath, bytes, cookedBytes, audioDesc)) {
                        std::cerr << "Failed to cook audio: " << asset.SourcePath.string() << "\n";
                        continue;
                    }
                } else {
                    cookedBytes = bytes;
                }

                const std::string           uuid      = ToStdString(asset.Uuid.ToNativeString());
                const std::string           cookedRel = "Assets/" + uuid + ".bin";
                const std::filesystem::path cookedPath =
                    paths.CookedRoot / "Assets" / (uuid + ".bin");

                const std::string cookKey = isMesh
                    ? BuildCookKeyWithExtras(bytes, cookKeyExtras, asset, platform)
                    : BuildCookKey(bytes, asset, platform);

                bool              needsCook = true;
                auto              cacheIt   = cacheEntries.find(uuid);
                if (cacheIt != cacheEntries.end()) {
                    if (cacheIt->second.CookKey == cookKey && std::filesystem::exists(cookedPath)) {
                        needsCook = false;
                    }
                }

                if (needsCook) {
                    std::error_code ec;
                    std::filesystem::create_directories(cookedPath.parent_path(), ec);
                    if (!WriteBytesFile(cookedPath, cookedBytes)) {
                        std::cerr << "Failed to write cooked asset: " << cookedPath.string()
                                  << "\n";
                        continue;
                    }
                    ++cookedCount;
                }

                FCookCacheEntry cacheEntry{};
                cacheEntry.Uuid       = uuid;
                cacheEntry.CookKey    = cookKey;
                cacheEntry.SourcePath = asset.SourcePathRel;
                cacheEntry.CookedPath = cookedRel;
                cacheEntry.LastCooked = GetUtcTimestamp();
                cacheEntries[uuid]    = cacheEntry;

                FRegistryEntry registryEntry{};
                registryEntry.Uuid        = uuid;
                registryEntry.Type        = asset.Type;
                registryEntry.VirtualPath = asset.VirtualPath;
                registryEntry.CookedPath  = cookedRel;
                if (isTexture) {
                    registryEntry.TextureDesc    = textureDesc;
                    registryEntry.HasTextureDesc = true;
                } else if (isMesh) {
                    registryEntry.MeshDesc    = meshDesc;
                    registryEntry.HasMeshDesc = true;
                } else if (isAudio) {
                    registryEntry.AudioDesc    = audioDesc;
                    registryEntry.HasAudioDesc = true;
                }
                registryAssets.push_back(registryEntry);
            }

            const std::filesystem::path registryPath =
                paths.CookedRoot / "Registry" / "AssetRegistry.json";
            if (!WriteRegistry(registryPath, registryAssets)) {
                std::cerr << "Failed to write registry: " << registryPath.string() << "\n";
                return 1;
            }

            if (!SaveCookCache(paths.CookCachePath, cacheEntries)) {
                std::cerr << "Failed to write cook cache: " << paths.CookCachePath.string() << "\n";
                return 1;
            }

            std::cout << "Cooked assets: " << cookedCount << "\n";
            std::cout << "Registry: " << registryPath.string() << "\n";
            return 0;
        }

        auto BundleAssets(const FCommandLine& command) -> int {
            const std::string platform = command.Options.contains("platform")
                ? command.Options.at("platform")
                : std::string("Win64");
            const std::string demoFilter =
                command.Options.contains("demo") ? command.Options.at("demo") : std::string();

            FToolPaths                  paths = BuildPaths(command, platform);
            const std::filesystem::path registryPath =
                paths.CookedRoot / "Registry" / "AssetRegistry.json";

            std::vector<FBundledAsset> assets;
            if (!LoadRegistryAssets(registryPath, paths.CookedRoot, assets)) {
                std::cerr << "Failed to load registry assets: " << registryPath.string() << "\n";
                return 1;
            }

            std::sort(assets.begin(), assets.end(),
                [](const FBundledAsset& left, const FBundledAsset& right) {
                    return left.UuidText < right.UuidText;
                });

            const std::string bundleName = demoFilter.empty() ? std::string("All") : demoFilter;
            const std::filesystem::path bundlePath =
                paths.CookedRoot / "Bundles" / (bundleName + ".pak");

            std::error_code ec;
            std::filesystem::create_directories(bundlePath.parent_path(), ec);

            std::ofstream file(bundlePath, std::ios::binary);
            if (!file) {
                std::cerr << "Failed to open bundle for writing: " << bundlePath.string() << "\n";
                return 1;
            }

            Asset::FBundleHeader header{};
            header.Magic   = Asset::kBundleMagic;
            header.Version = Asset::kBundleVersion;

            file.write(reinterpret_cast<const char*>(&header),
                static_cast<std::streamsize>(sizeof(header)));

            u64                                   offset = sizeof(header);
            std::vector<Asset::FBundleIndexEntry> entries;
            entries.reserve(assets.size());

            for (const auto& asset : assets) {
                if (asset.Data.empty()) {
                    std::cerr << "Skipping empty asset: " << asset.CookedPath << "\n";
                    continue;
                }

                Asset::FBundleIndexEntry entry{};
                WriteBundleUuid(entry, asset.Uuid);
                entry.Type             = static_cast<u32>(asset.Type);
                entry.Compression      = static_cast<u32>(Asset::EBundleCompression::None);
                entry.Offset           = offset;
                entry.Size             = static_cast<u64>(asset.Data.size());
                entry.RawSize          = static_cast<u64>(asset.Data.size());
                entry.ChunkCount       = 0;
                entry.ChunkTableOffset = 0;

                file.write(reinterpret_cast<const char*>(asset.Data.data()),
                    static_cast<std::streamsize>(asset.Data.size()));
                offset += entry.Size;
                entries.push_back(entry);
            }

            const u64                 indexOffset = offset;
            Asset::FBundleIndexHeader indexHeader{};
            indexHeader.EntryCount      = static_cast<u32>(entries.size());
            indexHeader.StringTableSize = 0;

            file.write(reinterpret_cast<const char*>(&indexHeader),
                static_cast<std::streamsize>(sizeof(indexHeader)));

            if (!entries.empty()) {
                file.write(reinterpret_cast<const char*>(entries.data()),
                    static_cast<std::streamsize>(
                        entries.size() * sizeof(Asset::FBundleIndexEntry)));
            }

            const u64 indexSize = sizeof(indexHeader)
                + static_cast<u64>(entries.size()) * sizeof(Asset::FBundleIndexEntry);
            const u64 bundleSize = indexOffset + indexSize;

            header.IndexOffset = indexOffset;
            header.IndexSize   = indexSize;
            header.BundleSize  = bundleSize;

            file.seekp(0, std::ios::beg);
            file.write(reinterpret_cast<const char*>(&header),
                static_cast<std::streamsize>(sizeof(header)));

            if (!file.good()) {
                std::cerr << "Failed to write bundle: " << bundlePath.string() << "\n";
                return 1;
            }

            std::cout << "Bundle: " << bundlePath.string() << "\n";
            std::cout << "Bundle assets: " << entries.size() << "\n";
            return 0;
        }
        auto ValidateRegistry(const std::filesystem::path& registryPath) -> int {
            std::string text;
            if (!ReadFileText(registryPath, text)) {
                std::cerr << "Failed to read registry: " << registryPath.string() << "\n";
                return 1;
            }

            FNativeString native;
            native.Append(text.c_str(), text.size());
            const FNativeStringView view(native.GetData(), native.Length());

            FJsonDocument           document;
            if (!document.Parse(view)) {
                std::cerr << "Registry JSON parse failed.\n";
                return 1;
            }

            const FJsonValue* root = document.GetRoot();
            if (root == nullptr || root->Type != EJsonType::Object) {
                std::cerr << "Registry root is invalid.\n";
                return 1;
            }

            const FJsonValue* schemaValue  = FindObjectValueInsensitive(*root, "SchemaVersion");
            double            schemaNumber = 0.0;
            if (!GetNumberValue(schemaValue, schemaNumber)) {
                std::cerr << "SchemaVersion missing or invalid.\n";
                return 1;
            }

            const FJsonValue* assetsValue = FindObjectValueInsensitive(*root, "Assets");
            if (assetsValue == nullptr || assetsValue->Type != EJsonType::Array) {
                std::cerr << "Assets array missing.\n";
                return 1;
            }

            std::unordered_set<std::string> uuidSet;
            std::unordered_set<std::string> pathSet;
            bool                            ok = true;

            for (const auto* assetValue : assetsValue->Array) {
                if (assetValue == nullptr || assetValue->Type != EJsonType::Object) {
                    std::cerr << "Asset entry is not an object.\n";
                    ok = false;
                    continue;
                }

                FNativeString uuidText;
                FNativeString typeText;
                FNativeString pathText;

                if (!GetStringValue(FindObjectValueInsensitive(*assetValue, "Uuid"), uuidText)
                    || !GetStringValue(FindObjectValueInsensitive(*assetValue, "Type"), typeText)
                    || !GetStringValue(
                        FindObjectValueInsensitive(*assetValue, "VirtualPath"), pathText)) {
                    std::cerr << "Asset missing required fields.\n";
                    ok = false;
                    continue;
                }

                std::string uuid  = ToStdString(uuidText);
                std::string type  = ToStdString(typeText);
                std::string vpath = ToStdString(pathText);
                ToLowerAscii(uuid);
                ToLowerAscii(vpath);

                if (ParseAssetType(type) == Asset::EAssetType::Unknown) {
                    std::cerr << "Unknown asset type: " << type << "\n";
                    ok = false;
                }

                if (!uuidSet.insert(uuid).second) {
                    std::cerr << "Duplicate UUID: " << uuid << "\n";
                    ok = false;
                }

                if (!pathSet.insert(vpath).second) {
                    std::cerr << "Duplicate VirtualPath: " << vpath << "\n";
                    ok = false;
                }
            }

            if (!ok) {
                return 1;
            }

            std::cout << "Registry validated. SchemaVersion=" << schemaNumber << "\n";
            return 0;
        }
        auto CleanCache(const FCommandLine& command) -> int {
            FToolPaths paths   = BuildPaths(command, "Win64");
            auto       cacheIt = command.Options.find("cache");
            if (cacheIt == command.Options.end()) {
                std::cerr << "Specify --cache to remove cook cache.\n";
                return 1;
            }

            std::error_code ec;
            if (std::filesystem::exists(paths.CookCachePath, ec)) {
                std::filesystem::remove(paths.CookCachePath, ec);
                if (ec) {
                    std::cerr << "Failed to remove cache: " << paths.CookCachePath.string() << "\n";
                    return 1;
                }
            }

            std::cout << "Cook cache removed.\n";
            return 0;
        }

    } // namespace

    auto RunTool(int argc, char** argv) -> int {
        FCommandLine command;
        std::string  error;
        if (!ParseCommandLine(argc, argv, command, error)) {
            std::cerr << error << "\n";
            PrintUsage();
            return 1;
        }

        const std::string cmdLower = [&command]() {
            std::string out = command.Command;
            ToLowerAscii(out);
            return out;
        }();

        if (cmdLower == "import") {
            return ImportAssets(command);
        }
        if (cmdLower == "cook") {
            return CookAssets(command);
        }
        if (cmdLower == "bundle") {
            return BundleAssets(command);
        }
        if (cmdLower == "validate") {
            auto registryIt = command.Options.find("registry");
            if (registryIt == command.Options.end()) {
                std::cerr << "Missing --registry.\n";
                return 1;
            }
            return ValidateRegistry(std::filesystem::path(registryIt->second));
        }
        if (cmdLower == "clean") {
            return CleanCache(command);
        }

        PrintUsage();
        return 1;
    }

} // namespace AltinaEngine::Tools::AssetPipeline

auto main(int argc, char** argv) -> int {
    return AltinaEngine::Tools::AssetPipeline::RunTool(argc, argv);
}
