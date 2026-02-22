#include "Imaging/ImageIO.h"

#if AE_PLATFORM_WIN
    #ifdef TEXT
        #undef TEXT
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <wincodec.h>
    #include <wrl/client.h>
    #include <objidl.h>
    #include <propidl.h>
    #include <oleauto.h>
    #include <combaseapi.h>
    #ifdef TEXT
        #undef TEXT
    #endif
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        #define TEXT(str) L##str
    #else
        #define TEXT(str) str
    #endif
#endif

#include <algorithm>
#include <limits>

namespace AltinaEngine::Imaging {
#if AE_PLATFORM_WIN
    namespace {
        using Microsoft::WRL::ComPtr;

        struct FComInitScope {
            HRESULT mResult       = E_FAIL;
            bool    mShouldUninit = false;

            FComInitScope() {
                mResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                if (mResult == S_OK || mResult == S_FALSE) {
                    mShouldUninit = true;
                } else if (mResult == RPC_E_CHANGED_MODE) {
                    mResult = S_OK;
                }
            }

            ~FComInitScope() {
                if (mShouldUninit) {
                    CoUninitialize();
                }
            }

            [[nodiscard]] auto IsOk() const noexcept -> bool { return SUCCEEDED(mResult); }
        };

        [[nodiscard]] auto FitsInDword(usize size) noexcept -> bool {
            return size <= static_cast<usize>(std::numeric_limits<DWORD>::max());
        }

        [[nodiscard]] auto FitsInUint(usize size) noexcept -> bool {
            return size <= static_cast<usize>(std::numeric_limits<UINT>::max());
        }

        [[nodiscard]] auto CreateWicFactory(ComPtr<IWICImagingFactory>& outFactory) -> bool {
            const HRESULT hr = CoCreateInstance(
                CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&outFactory));
            return SUCCEEDED(hr);
        }

        [[nodiscard]] auto DecodeWicImage(TSpan<u8> bytes, FImage& outImage) -> bool {
            if (bytes.IsEmpty() || !FitsInDword(bytes.Size())) {
                return false;
            }

            FComInitScope com;
            if (!com.IsOk()) {
                return false;
            }

            ComPtr<IWICImagingFactory> factory;
            if (!CreateWicFactory(factory)) {
                return false;
            }

            ComPtr<IWICStream> stream;
            HRESULT            hr = factory->CreateStream(&stream);
            if (FAILED(hr)) {
                return false;
            }

            hr = stream->InitializeFromMemory(
                const_cast<BYTE*>(bytes.Data()), static_cast<DWORD>(bytes.Size()));
            if (FAILED(hr)) {
                return false;
            }

            ComPtr<IWICBitmapDecoder> decoder;
            hr = factory->CreateDecoderFromStream(
                stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
            if (FAILED(hr)) {
                return false;
            }

            ComPtr<IWICBitmapFrameDecode> frame;
            hr = decoder->GetFrame(0, &frame);
            if (FAILED(hr)) {
                return false;
            }

            UINT width  = 0;
            UINT height = 0;
            hr          = frame->GetSize(&width, &height);
            if (FAILED(hr) || width == 0 || height == 0) {
                return false;
            }

            ComPtr<IWICFormatConverter> converter;
            hr = factory->CreateFormatConverter(&converter);
            if (FAILED(hr)) {
                return false;
            }

            hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
            if (FAILED(hr)) {
                return false;
            }

            outImage.Resize(width, height, EImageFormat::RGBA8);
            if (!outImage.IsValid()) {
                return false;
            }

            const auto dataSize = outImage.GetDataSize();
            if (!FitsInUint(dataSize)) {
                return false;
            }

            hr = converter->CopyPixels(
                nullptr, outImage.GetRowPitch(), static_cast<UINT>(dataSize), outImage.GetData());
            return SUCCEEDED(hr);
        }

        struct FEncodedPixels {
            const u8*          mData        = nullptr;
            u32                mRowPitch    = 0;
            WICPixelFormatGUID mPixelFormat = GUID_WICPixelFormatDontCare;
            TVector<u8>        mScratch;
        };

        [[nodiscard]] auto PreparePngPixels(const FImageView& image, FEncodedPixels& out) -> bool {
            if (!image.IsValid()) {
                return false;
            }

            const auto bytesPerPixel = image.GetBytesPerPixel();
            const auto minRowPitch   = image.mWidth * bytesPerPixel;
            if (bytesPerPixel == 0 || image.mRowPitch < minRowPitch) {
                return false;
            }

            if (image.mFormat == EImageFormat::RGBA8) {
                out.mRowPitch    = image.mWidth * 4;
                out.mPixelFormat = GUID_WICPixelFormat32bppBGRA;
                out.mScratch.Resize(static_cast<usize>(out.mRowPitch) * image.mHeight);

                for (u32 y = 0; y < image.mHeight; ++y) {
                    const u8* srcRow = image.GetRow(y);
                    u8*       dstRow = out.mScratch.Data() + static_cast<usize>(y) * out.mRowPitch;
                    for (u32 x = 0; x < image.mWidth; ++x) {
                        const u8* srcPixel = srcRow + static_cast<usize>(x) * 4;
                        u8*       dstPixel = dstRow + static_cast<usize>(x) * 4;
                        dstPixel[0]        = srcPixel[2];
                        dstPixel[1]        = srcPixel[1];
                        dstPixel[2]        = srcPixel[0];
                        dstPixel[3]        = srcPixel[3];
                    }
                }

                out.mData = out.mScratch.Data();
                return true;
            }

            if (image.mFormat == EImageFormat::RGB8) {
                out.mRowPitch    = image.mWidth * 3;
                out.mPixelFormat = GUID_WICPixelFormat24bppBGR;
                out.mScratch.Resize(static_cast<usize>(out.mRowPitch) * image.mHeight);

                for (u32 y = 0; y < image.mHeight; ++y) {
                    const u8* srcRow = image.GetRow(y);
                    u8*       dstRow = out.mScratch.Data() + static_cast<usize>(y) * out.mRowPitch;
                    for (u32 x = 0; x < image.mWidth; ++x) {
                        const u8* srcPixel = srcRow + static_cast<usize>(x) * 3;
                        u8*       dstPixel = dstRow + static_cast<usize>(x) * 3;
                        dstPixel[0]        = srcPixel[2];
                        dstPixel[1]        = srcPixel[1];
                        dstPixel[2]        = srcPixel[0];
                    }
                }

                out.mData = out.mScratch.Data();
                return true;
            }

            return false;
        }

        [[nodiscard]] auto PrepareJpegPixels(const FImageView& image, FEncodedPixels& out) -> bool {
            if (!image.IsValid()) {
                return false;
            }

            const auto bytesPerPixel = image.GetBytesPerPixel();
            const auto minRowPitch   = image.mWidth * bytesPerPixel;
            if (bytesPerPixel == 0 || image.mRowPitch < minRowPitch) {
                return false;
            }

            out.mRowPitch    = image.mWidth * 3;
            out.mPixelFormat = GUID_WICPixelFormat24bppBGR;
            out.mScratch.Resize(static_cast<usize>(out.mRowPitch) * image.mHeight);

            if (image.mFormat == EImageFormat::RGBA8) {
                for (u32 y = 0; y < image.mHeight; ++y) {
                    const u8* srcRow = image.GetRow(y);
                    u8*       dstRow = out.mScratch.Data() + static_cast<usize>(y) * out.mRowPitch;
                    for (u32 x = 0; x < image.mWidth; ++x) {
                        const u8* srcPixel = srcRow + static_cast<usize>(x) * 4;
                        u8*       dstPixel = dstRow + static_cast<usize>(x) * 3;
                        dstPixel[0]        = srcPixel[2];
                        dstPixel[1]        = srcPixel[1];
                        dstPixel[2]        = srcPixel[0];
                    }
                }
                out.mData = out.mScratch.Data();
                return true;
            }

            if (image.mFormat == EImageFormat::RGB8) {
                for (u32 y = 0; y < image.mHeight; ++y) {
                    const u8* srcRow = image.GetRow(y);
                    u8*       dstRow = out.mScratch.Data() + static_cast<usize>(y) * out.mRowPitch;
                    for (u32 x = 0; x < image.mWidth; ++x) {
                        const u8* srcPixel = srcRow + static_cast<usize>(x) * 3;
                        u8*       dstPixel = dstRow + static_cast<usize>(x) * 3;
                        dstPixel[0]        = srcPixel[2];
                        dstPixel[1]        = srcPixel[1];
                        dstPixel[2]        = srcPixel[0];
                    }
                }
                out.mData = out.mScratch.Data();
                return true;
            }

            return false;
        }

        [[nodiscard]] auto EncodeWicImage(const FEncodedPixels& pixels, u32 width, u32 height,
            REFGUID containerFormat, float quality, TVector<u8>& outBytes) -> bool {
            if (pixels.mData == nullptr || width == 0 || height == 0) {
                return false;
            }

            const usize dataSize = static_cast<usize>(pixels.mRowPitch) * height;
            if (!FitsInUint(dataSize)) {
                return false;
            }

            FComInitScope com;
            if (!com.IsOk()) {
                return false;
            }

            ComPtr<IWICImagingFactory> factory;
            if (!CreateWicFactory(factory)) {
                return false;
            }

            ComPtr<IStream> stream;
            HRESULT         hr = CreateStreamOnHGlobal(nullptr, TRUE, &stream);
            if (FAILED(hr)) {
                return false;
            }

            ComPtr<IWICBitmapEncoder> encoder;
            hr = factory->CreateEncoder(containerFormat, nullptr, &encoder);
            if (FAILED(hr)) {
                return false;
            }

            hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
            if (FAILED(hr)) {
                return false;
            }

            ComPtr<IWICBitmapFrameEncode> frame;
            ComPtr<IPropertyBag2>         bag;
            hr = encoder->CreateNewFrame(&frame, &bag);
            if (FAILED(hr)) {
                return false;
            }

            if (bag && quality >= 0.0f) {
                PROPBAG2 option{};
                option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
                VARIANT var;
                VariantInit(&var);
                var.vt     = VT_R4;
                var.fltVal = std::clamp(quality, 0.0f, 1.0f);
                bag->Write(1, &option, &var);
                VariantClear(&var);
            }

            hr = frame->Initialize(bag.Get());
            if (FAILED(hr)) {
                return false;
            }

            hr = frame->SetSize(width, height);
            if (FAILED(hr)) {
                return false;
            }

            WICPixelFormatGUID targetFormat = pixels.mPixelFormat;
            hr                              = frame->SetPixelFormat(&targetFormat);
            if (FAILED(hr) || targetFormat != pixels.mPixelFormat) {
                return false;
            }

            hr = frame->WritePixels(height, pixels.mRowPitch, static_cast<UINT>(dataSize),
                const_cast<BYTE*>(pixels.mData));
            if (FAILED(hr)) {
                return false;
            }

            hr = frame->Commit();
            if (FAILED(hr)) {
                return false;
            }

            hr = encoder->Commit();
            if (FAILED(hr)) {
                return false;
            }

            STATSTG stat{};
            hr = stream->Stat(&stat, STATFLAG_NONAME);
            if (FAILED(hr) || stat.cbSize.QuadPart <= 0
                || stat.cbSize.QuadPart > std::numeric_limits<ULONG>::max()) {
                return false;
            }

            const auto size = static_cast<ULONG>(stat.cbSize.QuadPart);
            outBytes.Resize(size);

            LARGE_INTEGER seek{};
            seek.QuadPart = 0;
            hr            = stream->Seek(seek, STREAM_SEEK_SET, nullptr);
            if (FAILED(hr)) {
                return false;
            }

            ULONG readBytes = 0;
            hr              = stream->Read(outBytes.Data(), size, &readBytes);
            if (FAILED(hr) || readBytes != size) {
                return false;
            }

            return true;
        }
    } // namespace
#endif

    auto FJpegImageReader::CanRead(TSpan<u8> bytes) const noexcept -> bool {
        return (bytes.Size() >= 2) && bytes[0] == 0xFF && bytes[1] == 0xD8;
    }

    auto FJpegImageReader::Read(TSpan<u8> bytes, FImage& outImage) const -> bool {
#if AE_PLATFORM_WIN
        if (!CanRead(bytes)) {
            return false;
        }
        return DecodeWicImage(bytes, outImage);
#else
        (void)bytes;
        (void)outImage;
        return false;
#endif
    }

    FJpegImageWriter::FJpegImageWriter(u8 quality) noexcept : mQuality(quality) {}

    void FJpegImageWriter::SetQuality(u8 quality) noexcept { mQuality = quality; }

    auto FJpegImageWriter::Write(const FImageView& image, TVector<u8>& outBytes) const -> bool {
#if AE_PLATFORM_WIN
        FEncodedPixels pixels;
        if (!PrepareJpegPixels(image, pixels)) {
            return false;
        }

        const float quality = static_cast<float>(mQuality) / 100.0f;
        return EncodeWicImage(
            pixels, image.mWidth, image.mHeight, GUID_ContainerFormatJpeg, quality, outBytes);
#else
        (void)image;
        outBytes.Clear();
        return false;
#endif
    }

    auto FPngImageReader::CanRead(TSpan<u8> bytes) const noexcept -> bool {
        static constexpr u8 kPngSignature[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
        if (bytes.Size() < 8) {
            return false;
        }
        for (usize i = 0; i < 8; ++i) {
            if (bytes[i] != kPngSignature[i]) {
                return false;
            }
        }
        return true;
    }

    auto FPngImageReader::Read(TSpan<u8> bytes, FImage& outImage) const -> bool {
#if AE_PLATFORM_WIN
        if (!CanRead(bytes)) {
            return false;
        }
        return DecodeWicImage(bytes, outImage);
#else
        (void)bytes;
        (void)outImage;
        return false;
#endif
    }

    auto FPngImageWriter::Write(const FImageView& image, TVector<u8>& outBytes) const -> bool {
#if AE_PLATFORM_WIN
        FEncodedPixels pixels;
        if (!PreparePngPixels(image, pixels)) {
            return false;
        }

        return EncodeWicImage(
            pixels, image.mWidth, image.mHeight, GUID_ContainerFormatPng, -1.0f, outBytes);
#else
        (void)image;
        outBytes.Clear();
        return false;
#endif
    }

} // namespace AltinaEngine::Imaging
