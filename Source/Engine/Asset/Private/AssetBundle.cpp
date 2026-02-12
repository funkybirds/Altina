#include "Asset/AssetBundle.h"

#include "Types/Traits.h"

#include <filesystem>
#include <limits>

namespace AltinaEngine::Asset {
    namespace {
        auto ToPath(const FString& value) -> std::filesystem::path {
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
            return std::filesystem::path(std::wstring(value.GetData(), value.Length()));
#else
            return std::filesystem::path(std::string(value.GetData(), value.Length()));
#endif
        }

        auto ReadExact(std::ifstream& stream, void* outBuffer, usize size) -> bool {
            if (outBuffer == nullptr || size == 0U) {
                return false;
            }

            stream.read(reinterpret_cast<char*>(outBuffer), static_cast<std::streamsize>(size));
            return static_cast<usize>(stream.gcount()) == size;
        }

        auto UuidMatches(const FBundleIndexEntry& entry, const FUuid& uuid) noexcept -> bool {
            const auto& bytes = uuid.GetBytes();
            for (usize index = 0; index < FUuid::kByteCount; ++index) {
                if (entry.Uuid[index] != bytes[index]) {
                    return false;
                }
            }
            return true;
        }
    } // namespace

    FAssetBundleReader::~FAssetBundleReader() {
        Close();
    }

    auto FAssetBundleReader::Open(const FString& path) -> bool {
        Close();

        mFile.open(ToPath(path), std::ios::binary);
        if (!mFile) {
            return false;
        }

        mFile.seekg(0, std::ios::end);
        const auto endPos = mFile.tellg();
        if (endPos < 0) {
            Close();
            return false;
        }
        mFileSize = static_cast<u64>(endPos);
        mFile.seekg(0, std::ios::beg);

        if (!ReadExact(mFile, &mHeader, sizeof(FBundleHeader))) {
            Close();
            return false;
        }

        if (mHeader.Magic != kBundleMagic || mHeader.Version != kBundleVersion) {
            Close();
            return false;
        }

        if (mHeader.BundleSize == 0U) {
            mHeader.BundleSize = mFileSize;
        }
        if (mHeader.BundleSize > mFileSize) {
            Close();
            return false;
        }

        if (mHeader.IndexOffset == 0U || mHeader.IndexSize == 0U) {
            Close();
            return false;
        }
        if (mHeader.IndexOffset + mHeader.IndexSize > mHeader.BundleSize) {
            Close();
            return false;
        }

        mFile.seekg(static_cast<std::streamoff>(mHeader.IndexOffset), std::ios::beg);
        FBundleIndexHeader indexHeader{};
        if (!ReadExact(mFile, &indexHeader, sizeof(FBundleIndexHeader))) {
            Close();
            return false;
        }

        const u64 entryBytes = static_cast<u64>(indexHeader.EntryCount)
            * sizeof(FBundleIndexEntry);
        if (sizeof(FBundleIndexHeader) + entryBytes > mHeader.IndexSize) {
            Close();
            return false;
        }

        mEntries.Clear();
        if (indexHeader.EntryCount > 0U) {
            mEntries.Resize(static_cast<usize>(indexHeader.EntryCount));
            if (!ReadExact(mFile, mEntries.Data(), static_cast<usize>(entryBytes))) {
                Close();
                return false;
            }
        }

        mFile.clear();
        return true;
    }

    void FAssetBundleReader::Close() {
        if (mFile.is_open()) {
            mFile.close();
        }
        mEntries.Clear();
        mHeader = {};
        mFileSize = 0U;
    }

    auto FAssetBundleReader::IsOpen() const noexcept -> bool {
        return mFile.is_open();
    }

    auto FAssetBundleReader::GetEntry(const FUuid& uuid, FBundleIndexEntry& outEntry) const noexcept
        -> bool {
        for (const auto& entry : mEntries) {
            if (UuidMatches(entry, uuid)) {
                outEntry = entry;
                return true;
            }
        }
        return false;
    }

    auto FAssetBundleReader::ReadEntry(
        const FBundleIndexEntry& entry, TVector<u8>& outBytes) const -> bool {
        outBytes.Clear();
        if (!mFile.is_open()) {
            return false;
        }

        if (entry.Compression != static_cast<u32>(EBundleCompression::None)) {
            return false;
        }
        if (entry.ChunkCount != 0U) {
            return false;
        }
        if (entry.Offset + entry.Size > mHeader.BundleSize) {
            return false;
        }
        if (entry.Size > static_cast<u64>(std::numeric_limits<usize>::max())) {
            return false;
        }

        outBytes.Resize(static_cast<usize>(entry.Size));
        mFile.clear();
        mFile.seekg(static_cast<std::streamoff>(entry.Offset), std::ios::beg);
        if (!ReadExact(mFile, outBytes.Data(), static_cast<usize>(entry.Size))) {
            outBytes.Clear();
            return false;
        }

        return true;
    }

} // namespace AltinaEngine::Asset
