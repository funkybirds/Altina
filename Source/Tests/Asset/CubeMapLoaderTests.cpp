#include "TestHarness.h"

#include "Asset/AssetBinary.h"
#include "Asset/AssetTypes.h"
#include "Asset/CubeMapAsset.h"
#include "Asset/CubeMapLoader.h"

#include <cstring>

namespace {
    using AltinaEngine::u32;
    using AltinaEngine::u64;
    using AltinaEngine::u8;
    using AltinaEngine::usize;
    namespace Container = AltinaEngine::Core::Container;
    using Container::TVector;

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
        usize     mSize   = 0U;
        usize     mOffset = 0U;
    };

    auto ComputePackedCubeSize(u32 baseSize, u32 mipCount, u32 bytesPerPixel) -> u64 {
        if (baseSize == 0U || mipCount == 0U || bytesPerPixel == 0U) {
            return 0ULL;
        }

        u32 size  = baseSize;
        u64 total = 0ULL;
        for (u32 mip = 0U; mip < mipCount; ++mip) {
            total += static_cast<u64>(size) * static_cast<u64>(size)
                * static_cast<u64>(bytesPerPixel) * 6ULL;
            size = (size > 1U) ? (size >> 1U) : 1U;
        }
        return total;
    }
} // namespace

TEST_CASE("Asset.CubeMap.Loader.Basic") {
    using namespace AltinaEngine::Asset;

    const u32 size          = 4U;
    const u32 mipCount      = 2U;
    const u32 format        = kTextureFormatRGBA8;
    const u32 bytesPerPixel = GetTextureBytesPerPixel(format);
    REQUIRE(bytesPerPixel == 4U);

    const u64 payloadSize = ComputePackedCubeSize(size, mipCount, bytesPerPixel);
    REQUIRE(payloadSize > 0ULL);

    FAssetBlobHeader header{};
    header.Type     = static_cast<u8>(EAssetType::CubeMap);
    header.DescSize = static_cast<u32>(sizeof(FCubeMapBlobDesc));
    header.DataSize = static_cast<u32>(payloadSize);

    FCubeMapBlobDesc blobDesc{};
    blobDesc.Size     = size;
    blobDesc.Format   = format;
    blobDesc.MipCount = mipCount;
    blobDesc.RowPitch = size * bytesPerPixel;

    TVector<u8> cooked;
    cooked.Resize(sizeof(header) + sizeof(blobDesc) + static_cast<usize>(payloadSize));

    u8* writePtr = cooked.Data();
    std::memcpy(writePtr, &header, sizeof(header));
    writePtr += sizeof(header);
    std::memcpy(writePtr, &blobDesc, sizeof(blobDesc));
    writePtr += sizeof(blobDesc);

    for (usize i = 0U; i < static_cast<usize>(payloadSize); ++i) {
        writePtr[i] = static_cast<u8>(i & 0xFFU);
    }

    FTestAssetStream stream(cooked);
    FCubeMapLoader   loader;

    FAssetDesc       desc{};
    desc.CubeMap.Size     = size;
    desc.CubeMap.MipCount = mipCount;
    desc.CubeMap.Format   = format;
    desc.CubeMap.SRGB     = false;

    auto asset = loader.Load(desc, stream);
    REQUIRE(asset);

    auto* cube = static_cast<FCubeMapAsset*>(asset.Get());
    REQUIRE(cube != nullptr);

    const auto& runtime = cube->GetDesc();
    REQUIRE_EQ(runtime.Size, size);
    REQUIRE_EQ(runtime.MipCount, mipCount);
    REQUIRE_EQ(runtime.Format, format);
    REQUIRE(runtime.SRGB == false);

    REQUIRE_EQ(static_cast<u64>(cube->GetPixels().Size()), payloadSize);
    REQUIRE_EQ(cube->GetPixels()[0], static_cast<u8>(0U));
    REQUIRE_EQ(cube->GetPixels()[1], static_cast<u8>(1U));
}
