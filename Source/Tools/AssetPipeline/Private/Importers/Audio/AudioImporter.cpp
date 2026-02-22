#include "Importers/Audio/AudioImporter.h"

#include "Asset/AssetBinary.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>

namespace AltinaEngine::Tools::AssetPipeline {
    namespace {
        auto ToLowerAscii(char value) -> char {
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
    } // namespace

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
} // namespace AltinaEngine::Tools::AssetPipeline
