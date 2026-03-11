#include "Asset/AudioLoader.h"

#include "Asset/AssetBinary.h"
#include "Asset/AudioAsset.h"
#include "Types/Traits.h"

using AltinaEngine::Forward;
using AltinaEngine::Move;
using AltinaEngine::Core::Container::DestroyPolymorphic;
namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    namespace {
        auto ReadExact(IAssetStream& stream, void* outBuffer, usize size) -> bool {
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

        auto ReadHeader(IAssetStream& stream, FAssetBlobHeader& outHeader) -> bool {
            if (!ReadExact(stream, &outHeader, sizeof(FAssetBlobHeader))) {
                return false;
            }

            if (outHeader.mMagic != kAssetBlobMagic || outHeader.mVersion != kAssetBlobVersion) {
                return false;
            }

            if (outHeader.mType != static_cast<u8>(EAssetType::Audio)) {
                return false;
            }

            if (outHeader.mDescSize != sizeof(FAudioBlobDesc)) {
                return false;
            }

            return true;
        }

        auto RangeWithin(u64 offset, u64 size, u64 dataSize) noexcept -> bool {
            return offset <= dataSize && size <= (dataSize - offset);
        }

        template <typename TDerived, typename... Args>
        auto MakeSharedAsset(Args&&... args) -> TShared<IAsset> {
            return Container::MakeSharedAs<IAsset, TDerived>(Forward<Args>(args)...);
        }
    } // namespace

    auto FAudioLoader::CanLoad(EAssetType type) const noexcept -> bool {
        return type == EAssetType::Audio;
    }

    auto FAudioLoader::Load(const FAssetDesc& desc, IAssetStream& stream) -> TShared<IAsset> {
        FAssetBlobHeader header{};
        if (!ReadHeader(stream, header)) {
            return {};
        }

        FAudioBlobDesc blobDesc{};
        if (!ReadExact(stream, &blobDesc, sizeof(FAudioBlobDesc))) {
            return {};
        }

        if (blobDesc.mChannels == 0U || blobDesc.mSampleRate == 0U) {
            return {};
        }

        if (blobDesc.mDataSize == 0U || blobDesc.mFrameCount == 0U) {
            return {};
        }

        const u32 bytesPerSample = GetAudioBytesPerSample(blobDesc.mSampleFormat);
        if (bytesPerSample == 0U) {
            return {};
        }

        if (blobDesc.mChunkCount > 0U && blobDesc.mFramesPerChunk == 0U) {
            return {};
        }

        const u64 dataSize = header.mDataSize;
        const u64 chunkTableBytes =
            static_cast<u64>(blobDesc.mChunkCount) * sizeof(FAudioChunkDesc);
        if (blobDesc.mChunkCount > 0U) {
            if (!RangeWithin(blobDesc.mChunkTableOffset, chunkTableBytes, dataSize)) {
                return {};
            }
            if (blobDesc.mDataOffset < blobDesc.mChunkTableOffset + chunkTableBytes) {
                return {};
            }
        }

        if (!RangeWithin(blobDesc.mDataOffset, blobDesc.mDataSize, dataSize)) {
            return {};
        }

        if (blobDesc.mCodec == kAudioCodecPcm) {
            const u64 expectedSize = static_cast<u64>(blobDesc.mFrameCount)
                * static_cast<u64>(blobDesc.mChannels) * bytesPerSample;
            if (expectedSize != blobDesc.mDataSize) {
                return {};
            }
        }

        if (desc.mAudio.Codec != 0U && desc.mAudio.Codec != blobDesc.mCodec) {
            return {};
        }
        if (desc.mAudio.Channels != 0U && desc.mAudio.Channels != blobDesc.mChannels) {
            return {};
        }
        if (desc.mAudio.SampleRate != 0U && desc.mAudio.SampleRate != blobDesc.mSampleRate) {
            return {};
        }

        const usize baseOffset = stream.Tell();
        const u64   totalSize  = static_cast<u64>(baseOffset) + dataSize;
        const u64   streamSize = stream.Size();
        if (streamSize != 0U && totalSize > streamSize) {
            return {};
        }

        TVector<FAudioChunkDesc> chunks;
        if (blobDesc.mChunkCount > 0U) {
            chunks.Resize(static_cast<usize>(blobDesc.mChunkCount));
            stream.Seek(baseOffset + static_cast<usize>(blobDesc.mChunkTableOffset));
            if (!ReadExact(stream, chunks.Data(), static_cast<usize>(chunkTableBytes))) {
                return {};
            }

            const u64 dataStart       = blobDesc.mDataOffset;
            const u64 dataEnd         = blobDesc.mDataOffset + blobDesc.mDataSize;
            u64       totalChunkBytes = 0U;
            for (const auto& chunk : chunks) {
                const u64 offset = chunk.mOffset;
                const u64 size   = chunk.mSize;
                if (size == 0U) {
                    return {};
                }
                if (offset < dataStart || (offset + size) > dataEnd) {
                    return {};
                }
                totalChunkBytes += size;
                if (totalChunkBytes > blobDesc.mDataSize) {
                    return {};
                }
            }
            if (blobDesc.mCodec == kAudioCodecPcm && totalChunkBytes != blobDesc.mDataSize) {
                return {};
            }
        }

        TVector<u8> data;
        data.Resize(static_cast<usize>(blobDesc.mDataSize));
        stream.Seek(baseOffset + static_cast<usize>(blobDesc.mDataOffset));
        if (!ReadExact(stream, data.Data(), static_cast<usize>(blobDesc.mDataSize))) {
            return {};
        }

        FAudioRuntimeDesc runtimeDesc{};
        runtimeDesc.mCodec          = blobDesc.mCodec;
        runtimeDesc.mSampleFormat   = blobDesc.mSampleFormat;
        runtimeDesc.mChannels       = blobDesc.mChannels;
        runtimeDesc.mSampleRate     = blobDesc.mSampleRate;
        runtimeDesc.mFrameCount     = blobDesc.mFrameCount;
        runtimeDesc.mFramesPerChunk = blobDesc.mFramesPerChunk;

        return MakeSharedAsset<FAudioAsset>(runtimeDesc, Move(chunks), Move(data));
    }

} // namespace AltinaEngine::Asset
