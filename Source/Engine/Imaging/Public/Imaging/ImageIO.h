#pragma once

#include "ImagingAPI.h"
#include "Imaging/Image.h"

#include "Container/Span.h"
#include "Container/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Imaging {
    using Core::Container::TSpan;
    using Core::Container::TVector;

    class AE_IMAGING_API FImageReader {
    public:
        virtual ~FImageReader() = default;

        [[nodiscard]] virtual auto CanRead(TSpan<u8> bytes) const noexcept -> bool = 0;
        [[nodiscard]] virtual auto Read(TSpan<u8> bytes, FImage& outImage) const -> bool = 0;
    };

    class AE_IMAGING_API FImageWriter {
    public:
        virtual ~FImageWriter() = default;

        [[nodiscard]] virtual auto Write(const FImageView& image, TVector<u8>& outBytes) const -> bool = 0;
    };

    class AE_IMAGING_API FJpegImageReader final : public FImageReader {
    public:
        [[nodiscard]] auto CanRead(TSpan<u8> bytes) const noexcept -> bool override;
        [[nodiscard]] auto Read(TSpan<u8> bytes, FImage& outImage) const -> bool override;
    };

    class AE_IMAGING_API FJpegImageWriter final : public FImageWriter {
    public:
        explicit FJpegImageWriter(u8 quality = 90) noexcept;

        void SetQuality(u8 quality) noexcept;

        [[nodiscard]] auto Write(const FImageView& image, TVector<u8>& outBytes) const -> bool override;

    private:
        u8 mQuality = 90;
    };

    class AE_IMAGING_API FPngImageReader final : public FImageReader {
    public:
        [[nodiscard]] auto CanRead(TSpan<u8> bytes) const noexcept -> bool override;
        [[nodiscard]] auto Read(TSpan<u8> bytes, FImage& outImage) const -> bool override;
    };

    class AE_IMAGING_API FPngImageWriter final : public FImageWriter {
    public:
        [[nodiscard]] auto Write(const FImageView& image, TVector<u8>& outBytes) const -> bool override;
    };

} // namespace AltinaEngine::Imaging
