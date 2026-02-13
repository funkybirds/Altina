#include "Asset/AudioLoader.h"

#include "Asset/AssetBinary.h"
#include "Asset/AudioAsset.h"
#include "Types/Traits.h"

#include <limits>

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

            if (outHeader.Magic != kAssetBlobMagic || outHeader.Version != kAssetBlobVersion) {
                return false;
            }

            if (outHeader.Type != static_cast<u8>(EAssetType::Audio)) {
                return false;
            }

            if (outHeader.DescSize != sizeof(FAudioBlobDesc)) {
                return false;
            }

            return true;
        }

        auto RangeWithin(u64 offset, u64 size, u64 dataSize) noexcept -> bool {
            return offset <= dataSize && size <= (dataSize - offset);
        }

        template <typename TDerived, typename... Args>
        auto MakeSharedAsset(Args&&... args) -> TShared<IAsset> {
            using Container::kSmartPtrUseManagedAllocator;
            using Container::TAllocator;
            using Container::TAllocatorTraits;
            using Container::TPolymorphicDeleter;

            TDerived* ptr = nullptr;
            if constexpr (kSmartPtrUseManagedAllocator) {
                TAllocator<TDerived> allocator;
                ptr = TAllocatorTraits<TAllocator<TDerived>>::Allocate(allocator, 1);
                if (ptr == nullptr) {
                    return {};
                }

                try {
                    TAllocatorTraits<TAllocator<TDerived>>::Construct(
                        allocator, ptr, AltinaEngine::Forward<Args>(args)...);
                } catch (...) {
                    TAllocatorTraits<TAllocator<TDerived>>::Deallocate(allocator, ptr, 1);
                    return {};
                }
            } else {
                ptr = new TDerived(AltinaEngine::Forward<Args>(args)...); // NOLINT
            }

            return TShared<IAsset>(
                ptr, TPolymorphicDeleter<IAsset>(&Container::DestroyPolymorphic<IAsset, TDerived>));
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

        if (blobDesc.Channels == 0U || blobDesc.SampleRate == 0U) {
            return {};
        }

        if (blobDesc.DataSize == 0U || blobDesc.FrameCount == 0U) {
            return {};
        }

        const u32 bytesPerSample = GetAudioBytesPerSample(blobDesc.SampleFormat);
        if (bytesPerSample == 0U) {
            return {};
        }

        if (blobDesc.ChunkCount > 0U && blobDesc.FramesPerChunk == 0U) {
            return {};
        }

        const u64 dataSize        = header.DataSize;
        const u64 chunkTableBytes = static_cast<u64>(blobDesc.ChunkCount) * sizeof(FAudioChunkDesc);
        if (blobDesc.ChunkCount > 0U) {
            if (!RangeWithin(blobDesc.ChunkTableOffset, chunkTableBytes, dataSize)) {
                return {};
            }
            if (blobDesc.DataOffset < blobDesc.ChunkTableOffset + chunkTableBytes) {
                return {};
            }
        }

        if (!RangeWithin(blobDesc.DataOffset, blobDesc.DataSize, dataSize)) {
            return {};
        }

        if (blobDesc.Codec == kAudioCodecPcm) {
            const u64 expectedSize = static_cast<u64>(blobDesc.FrameCount)
                * static_cast<u64>(blobDesc.Channels) * bytesPerSample;
            if (expectedSize != blobDesc.DataSize) {
                return {};
            }
        }

        if (desc.Audio.Codec != 0U && desc.Audio.Codec != blobDesc.Codec) {
            return {};
        }
        if (desc.Audio.Channels != 0U && desc.Audio.Channels != blobDesc.Channels) {
            return {};
        }
        if (desc.Audio.SampleRate != 0U && desc.Audio.SampleRate != blobDesc.SampleRate) {
            return {};
        }

        const usize baseOffset = stream.Tell();
        const u64   totalSize  = static_cast<u64>(baseOffset) + dataSize;
        const u64   streamSize = stream.Size();
        if (streamSize != 0U && totalSize > streamSize) {
            return {};
        }

        TVector<FAudioChunkDesc> chunks;
        if (blobDesc.ChunkCount > 0U) {
            chunks.Resize(static_cast<usize>(blobDesc.ChunkCount));
            stream.Seek(baseOffset + static_cast<usize>(blobDesc.ChunkTableOffset));
            if (!ReadExact(stream, chunks.Data(), static_cast<usize>(chunkTableBytes))) {
                return {};
            }

            const u64 dataStart       = blobDesc.DataOffset;
            const u64 dataEnd         = blobDesc.DataOffset + blobDesc.DataSize;
            u64       totalChunkBytes = 0U;
            for (const auto& chunk : chunks) {
                const u64 offset = chunk.Offset;
                const u64 size   = chunk.Size;
                if (size == 0U) {
                    return {};
                }
                if (offset < dataStart || (offset + size) > dataEnd) {
                    return {};
                }
                totalChunkBytes += size;
                if (totalChunkBytes > blobDesc.DataSize) {
                    return {};
                }
            }
            if (blobDesc.Codec == kAudioCodecPcm && totalChunkBytes != blobDesc.DataSize) {
                return {};
            }
        }

        TVector<u8> data;
        data.Resize(static_cast<usize>(blobDesc.DataSize));
        stream.Seek(baseOffset + static_cast<usize>(blobDesc.DataOffset));
        if (!ReadExact(stream, data.Data(), static_cast<usize>(blobDesc.DataSize))) {
            return {};
        }

        FAudioRuntimeDesc runtimeDesc{};
        runtimeDesc.Codec          = blobDesc.Codec;
        runtimeDesc.SampleFormat   = blobDesc.SampleFormat;
        runtimeDesc.Channels       = blobDesc.Channels;
        runtimeDesc.SampleRate     = blobDesc.SampleRate;
        runtimeDesc.FrameCount     = blobDesc.FrameCount;
        runtimeDesc.FramesPerChunk = blobDesc.FramesPerChunk;

        return MakeSharedAsset<FAudioAsset>(
            runtimeDesc, AltinaEngine::Move(chunks), AltinaEngine::Move(data));
    }

} // namespace AltinaEngine::Asset
