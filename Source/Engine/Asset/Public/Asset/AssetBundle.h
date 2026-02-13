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
        u32 Magic       = kBundleMagic;
        u16 Version     = kBundleVersion;
        u16 Flags       = 0;
        u64 IndexOffset = 0;
        u64 IndexSize   = 0;
        u64 BundleSize  = 0;
        u64 HashOffset  = 0;
    };

    struct AE_ASSET_API FBundleIndexHeader {
        u32 EntryCount      = 0;
        u32 StringTableSize = 0;
    };

    struct AE_ASSET_API FBundleIndexEntry {
        u8  Uuid[AltinaEngine::FUuid::kByteCount]{};
        u32 Type             = 0;
        u32 Compression      = 0;
        u64 Offset           = 0;
        u64 Size             = 0;
        u64 RawSize          = 0;
        u32 ChunkCount       = 0;
        u32 ChunkTableOffset = 0;
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
