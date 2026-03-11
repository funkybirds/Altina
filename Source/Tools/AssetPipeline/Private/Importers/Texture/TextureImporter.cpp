#include "Importers/Texture/TextureImporter.h"

#include "Asset/AssetBinary.h"
#include "Container/Span.h"
#include "Container/Vector.h"
#include "Imaging/ImageIO.h"

#include <cstring>
#include <limits>

namespace AltinaEngine::Tools::AssetPipeline {
    namespace Container = Core::Container;
    namespace {
        using Container::TSpan;
        using Container::TVector;

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
    } // namespace

    auto CookTexture2D(const std::vector<u8>& sourceBytes, bool srgb, std::vector<u8>& outCooked,
        Asset::FTexture2DDesc& outDesc) -> bool {
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
        blobDesc.mWidth    = image.GetWidth();
        blobDesc.mHeight   = image.GetHeight();
        blobDesc.mFormat   = static_cast<u32>(image.GetFormat());
        blobDesc.mMipCount = 1;
        blobDesc.mRowPitch = image.GetRowPitch();

        const u32 bytesPerPixel = Asset::GetTextureBytesPerPixel(blobDesc.mFormat);
        if (bytesPerPixel == 0 || blobDesc.mRowPitch < blobDesc.mWidth * bytesPerPixel) {
            return false;
        }

        const u64 expectedSize = static_cast<u64>(blobDesc.mRowPitch) * blobDesc.mHeight;
        if (expectedSize != dataSize) {
            return false;
        }

        Asset::FAssetBlobHeader header{};
        header.mType     = static_cast<u8>(Asset::EAssetType::Texture2D);
        header.mFlags    = Asset::MakeAssetBlobFlags(srgb);
        header.mDescSize = static_cast<u32>(sizeof(Asset::FTexture2DBlobDesc));
        header.mDataSize = static_cast<u32>(dataSize);

        const usize totalSize =
            sizeof(Asset::FAssetBlobHeader) + sizeof(Asset::FTexture2DBlobDesc) + dataSize;
        outCooked.resize(totalSize);

        std::memcpy(outCooked.data(), &header, sizeof(Asset::FAssetBlobHeader));
        std::memcpy(outCooked.data() + sizeof(Asset::FAssetBlobHeader), &blobDesc,
            sizeof(Asset::FTexture2DBlobDesc));
        std::memcpy(
            outCooked.data() + sizeof(Asset::FAssetBlobHeader) + sizeof(Asset::FTexture2DBlobDesc),
            image.GetData(), dataSize);

        outDesc.Width    = blobDesc.mWidth;
        outDesc.Height   = blobDesc.mHeight;
        outDesc.Format   = blobDesc.mFormat;
        outDesc.MipCount = blobDesc.mMipCount;
        outDesc.SRGB     = srgb;

        return true;
    }
} // namespace AltinaEngine::Tools::AssetPipeline
