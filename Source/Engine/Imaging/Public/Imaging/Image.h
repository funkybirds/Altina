#pragma once

#include "ImagingAPI.h"

#include "Container/Vector.h"
#include "Types/Aliases.h"

#include <limits>

namespace AltinaEngine::Imaging {
    using Core::Container::TVector;

    enum class EImageFormat : u8 {
        Unknown = 0,
        R8,
        RGB8,
        RGBA8
    };

    [[nodiscard]] constexpr auto GetBytesPerPixel(EImageFormat format) noexcept -> u32 {
        switch (format) {
            case EImageFormat::R8:
                return 1;
            case EImageFormat::RGB8:
                return 3;
            case EImageFormat::RGBA8:
                return 4;
            default:
                return 0;
        }
    }

    struct AE_IMAGING_API FImageView {
        const u8*     mData     = nullptr;
        u32           mWidth    = 0;
        u32           mHeight   = 0;
        u32           mRowPitch = 0;
        EImageFormat  mFormat   = EImageFormat::Unknown;

        constexpr FImageView() noexcept = default;

        constexpr FImageView(
            const u8* data, u32 width, u32 height, EImageFormat format, u32 rowPitch = 0) noexcept
            : mData(data)
            , mWidth(width)
            , mHeight(height)
            , mRowPitch(rowPitch == 0
                    ? width * AltinaEngine::Imaging::GetBytesPerPixel(format)
                    : rowPitch)
            , mFormat(format) {}

        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool {
            return (mData != nullptr) && (mWidth > 0) && (mHeight > 0)
                && (GetBytesPerPixel() > 0) && (mRowPitch > 0);
        }

        [[nodiscard]] constexpr auto GetBytesPerPixel() const noexcept -> u32 {
            return AltinaEngine::Imaging::GetBytesPerPixel(mFormat);
        }

        [[nodiscard]] constexpr auto GetDataSize() const noexcept -> usize {
            return static_cast<usize>(mRowPitch) * static_cast<usize>(mHeight);
        }

        [[nodiscard]] constexpr auto GetRow(u32 row) const noexcept -> const u8* {
            if (mData == nullptr || row >= mHeight) {
                return nullptr;
            }
            return mData + static_cast<usize>(row) * static_cast<usize>(mRowPitch);
        }
    };

    class AE_IMAGING_API FImage {
    public:
        FImage() = default;

        FImage(u32 width, u32 height, EImageFormat format) { Resize(width, height, format); }

        void Reset() noexcept {
            mWidth    = 0;
            mHeight   = 0;
            mRowPitch = 0;
            mFormat   = EImageFormat::Unknown;
            mData.Clear();
        }

        void Resize(u32 width, u32 height, EImageFormat format) {
            const auto bytesPerPixel = GetBytesPerPixel(format);
            if (width == 0 || height == 0 || bytesPerPixel == 0) {
                Reset();
                return;
            }

            const u64 rowPitch64 = static_cast<u64>(width) * bytesPerPixel;
            const u64 size64     = rowPitch64 * static_cast<u64>(height);
            if (rowPitch64 > std::numeric_limits<u32>::max()
                || size64 > std::numeric_limits<usize>::max()) {
                Reset();
                return;
            }

            mWidth    = width;
            mHeight   = height;
            mRowPitch = static_cast<u32>(rowPitch64);
            mFormat   = format;
            mData.Resize(static_cast<usize>(size64));
        }

        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool {
            return (mWidth > 0) && (mHeight > 0) && (GetBytesPerPixel(mFormat) > 0)
                && (mData.Size() > 0);
        }

        [[nodiscard]] constexpr auto GetWidth() const noexcept -> u32 { return mWidth; }

        [[nodiscard]] constexpr auto GetHeight() const noexcept -> u32 { return mHeight; }

        [[nodiscard]] constexpr auto GetRowPitch() const noexcept -> u32 { return mRowPitch; }

        [[nodiscard]] constexpr auto GetFormat() const noexcept -> EImageFormat { return mFormat; }

        [[nodiscard]] constexpr auto GetDataSize() const noexcept -> usize { return mData.Size(); }

        [[nodiscard]] auto GetData() noexcept -> u8* { return mData.IsEmpty() ? nullptr : mData.Data(); }

        [[nodiscard]] auto GetData() const noexcept -> const u8* {
            return mData.IsEmpty() ? nullptr : mData.Data();
        }

        [[nodiscard]] auto View() const noexcept -> FImageView {
            return FImageView(GetData(), mWidth, mHeight, mFormat, mRowPitch);
        }

    private:
        u32          mWidth    = 0;
        u32          mHeight   = 0;
        u32          mRowPitch = 0;
        EImageFormat mFormat   = EImageFormat::Unknown;
        TVector<u8>  mData;
    };

} // namespace AltinaEngine::Imaging
