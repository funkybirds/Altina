#pragma once

#include "Asset/AssetTypes.h"
#include "Container/String.h"
#include "Container/Vector.h"
#include "Types/Aliases.h"
#include "Utility/Uuid.h"

#include <fstream>

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::TVector;

    constexpr u32 kBundleMagic   = 0x31424541u; // "AEB1"
    constexpr u16 kBundleVersion = 1;

    enum class EBundleFlags : u16 {
        None           = 0,
        HasChunks      = 1 << 0,
        HasCompression = 1 << 1,
        HasEncryption  = 1 << 2,
        HasHashTable   = 1 << 3
    };

    [[nodiscard]] constexpr auto HasBundleFlag(u16 flags, EBundleFlags flag) noexcept -> bool {
        return (flags & static_cast<u16>(flag)) != 0;
    }

    enum class EBundleCompression : u32 {
        None = 0,
        Lz4  = 1,
        Zstd = 2
    };

#pragma pack(push, 1)
    struct AE_ASSET_API FBundleHeader {
        u32 mMagic       = kBundleMagic;
        u16 mVersion     = kBundleVersion;
        u16 mFlags       = 0;
        u64 mIndexOffset = 0;
        u64 mIndexSize   = 0;
        u64 mBundleSize  = 0;
        u64 mHashOffset  = 0;
    };

    struct AE_ASSET_API FBundleIndexHeader {
        u32 mEntryCount      = 0;
        u32 mStringTableSize = 0;
    };

    struct AE_ASSET_API FBundleIndexEntry {
        u8  mUuid[FUuid::kByteCount]{};
        u32 mType             = 0;
        u32 mCompression      = 0;
        u64 mOffset           = 0;
        u64 mSize             = 0;
        u64 mRawSize          = 0;
        u32 mChunkCount       = 0;
        u32 mChunkTableOffset = 0;
    };

    struct AE_ASSET_API FBundleChunkDesc {
        u64 Offset  = 0;
        u64 Size    = 0;
        u64 RawSize = 0;
    };
#pragma pack(pop)

    class AE_ASSET_API FAssetBundleReader final {
    public:
        FAssetBundleReader() = default;
        ~FAssetBundleReader();

        auto               Open(const FString& path) -> bool;
        void               Close();
        [[nodiscard]] auto IsOpen() const noexcept -> bool;

        [[nodiscard]] auto GetHeader() const noexcept -> const FBundleHeader& { return mHeader; }

        auto GetEntry(const FUuid& uuid, FBundleIndexEntry& outEntry) const noexcept -> bool;
        auto ReadEntry(const FBundleIndexEntry& entry, TVector<u8>& outBytes) const -> bool;

    private:
        mutable std::ifstream      mFile;
        FBundleHeader              mHeader{};
        TVector<FBundleIndexEntry> mEntries;
        u64                        mFileSize = 0;
    };

} // namespace AltinaEngine::Asset
