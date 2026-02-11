#include "TestHarness.h"

#include "Imaging/ImageIO.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace AltinaEngine::Imaging {
    using Core::Container::TVector;

    static void HsvToRgb(f32 h, f32 s, f32 v, u8& outR, u8& outG, u8& outB) {
        const f32 c = v * s;
        const f32 h6 = h * 6.0f;
        const f32 x = c * (1.0f - std::fabs(std::fmod(h6, 2.0f) - 1.0f));
        f32 r1 = 0.0f;
        f32 g1 = 0.0f;
        f32 b1 = 0.0f;

        if (h6 >= 0.0f && h6 < 1.0f) {
            r1 = c;
            g1 = x;
        } else if (h6 < 2.0f) {
            r1 = x;
            g1 = c;
        } else if (h6 < 3.0f) {
            g1 = c;
            b1 = x;
        } else if (h6 < 4.0f) {
            g1 = x;
            b1 = c;
        } else if (h6 < 5.0f) {
            r1 = x;
            b1 = c;
        } else {
            r1 = c;
            b1 = x;
        }

        const f32 m = v - c;
        outR = static_cast<u8>(std::clamp((r1 + m) * 255.0f, 0.0f, 255.0f));
        outG = static_cast<u8>(std::clamp((g1 + m) * 255.0f, 0.0f, 255.0f));
        outB = static_cast<u8>(std::clamp((b1 + m) * 255.0f, 0.0f, 255.0f));
    }

    static auto MakeHsvDiskImage(u32 size) -> FImage {
        FImage image(size, size, EImageFormat::RGBA8);
        if (!image.IsValid()) {
            return image;
        }

        const f32 radius = static_cast<f32>(size) * 0.5f;
        const f32 center = radius - 0.5f;
        const f32 invRadius = radius > 0.0f ? 1.0f / radius : 0.0f;
        u8*       data = image.GetData();
        const u32 pitch = image.GetRowPitch();

        for (u32 y = 0; y < size; ++y) {
            for (u32 x = 0; x < size; ++x) {
                const f32 dx = (static_cast<f32>(x) - center) * invRadius;
                const f32 dy = (static_cast<f32>(y) - center) * invRadius;
                const f32 dist = std::sqrt(dx * dx + dy * dy);

                u8* pixel = data + static_cast<usize>(y) * pitch + static_cast<usize>(x) * 4;
                if (dist <= 1.0f) {
                    const f32 angle = std::atan2(dy, dx);
                    const f32 hue = (angle + 3.1415926535f) / (2.0f * 3.1415926535f);
                    const f32 sat = dist;
                    u8 r = 0;
                    u8 g = 0;
                    u8 b = 0;
                    HsvToRgb(hue, sat, 1.0f, r, g, b);
                    pixel[0] = r;
                    pixel[1] = g;
                    pixel[2] = b;
                    pixel[3] = 255;
                } else {
                    pixel[0] = 0;
                    pixel[1] = 0;
                    pixel[2] = 0;
                    pixel[3] = 255;
                }
            }
        }

        return image;
    }

    static auto WriteFileBytes(const std::filesystem::path& path, const TVector<u8>& bytes) -> bool {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file) {
            return false;
        }
        if (bytes.IsEmpty()) {
            return false;
        }
        file.write(reinterpret_cast<const char*>(bytes.Data()),
            static_cast<std::streamsize>(bytes.Size()));
        return file.good();
    }

    static auto ReadFileBytes(const std::filesystem::path& path, TVector<u8>& outBytes) -> bool {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            return false;
        }
        const std::streamoff size = file.tellg();
        if (size <= 0) {
            return false;
        }
        file.seekg(0, std::ios::beg);
        outBytes.Resize(static_cast<usize>(size));
        file.read(reinterpret_cast<char*>(outBytes.Data()), static_cast<std::streamsize>(size));
        return file.good();
    }

    static auto MakeTestImage() -> FImage {
        FImage image(4, 4, EImageFormat::RGBA8);
        u8*   data   = image.GetData();
        const u32 pitch  = image.GetRowPitch();

        for (u32 y = 0; y < image.GetHeight(); ++y) {
            for (u32 x = 0; x < image.GetWidth(); ++x) {
                u8* pixel = data + static_cast<usize>(y) * pitch + static_cast<usize>(x) * 4;
                pixel[0]  = static_cast<u8>(30 + x * 40);
                pixel[1]  = static_cast<u8>(50 + y * 30);
                pixel[2]  = static_cast<u8>(100 + x * 20 + y * 10);
                pixel[3]  = 255;
            }
        }

        return image;
    }

    static void RequireImagesEqual(const FImage& a, const FImage& b) {
        REQUIRE_EQ(a.GetWidth(), b.GetWidth());
        REQUIRE_EQ(a.GetHeight(), b.GetHeight());
        REQUIRE_EQ(a.GetFormat(), b.GetFormat());

        const auto bytes = a.GetDataSize();
        REQUIRE_EQ(bytes, b.GetDataSize());

        const u8* aData = a.GetData();
        const u8* bData = b.GetData();
        if (bytes == 0 || aData == nullptr || bData == nullptr) {
            REQUIRE(false);
            return;
        }
        for (usize i = 0; i < bytes; ++i) {
            REQUIRE_EQ(aData[i], bData[i]);
        }
    }

#if AE_PLATFORM_WIN
    TEST_CASE("Imaging PNG roundtrip") {
        FImage image = MakeTestImage();

        FPngImageWriter writer;
        TVector<u8>     bytes;
        if (!writer.Write(image.View(), bytes)) {
            REQUIRE(false);
            return;
        }
        if (!(bytes.Size() > 8)) {
            REQUIRE(false);
            return;
        }

        FPngImageReader reader;
        FImage          decoded;
        if (!reader.Read(Core::Container::TSpan<u8>(bytes), decoded)) {
            REQUIRE(false);
            return;
        }
        RequireImagesEqual(image, decoded);
    }

    TEST_CASE("Imaging JPEG roundtrip") {
        FImage image = MakeTestImage();

        FJpegImageWriter writer(90);
        TVector<u8>      bytes;
        if (!writer.Write(image.View(), bytes)) {
            REQUIRE(false);
            return;
        }
        if (!(bytes.Size() > 0)) {
            REQUIRE(false);
            return;
        }

        FJpegImageReader reader;
        FImage           decoded;
        if (!reader.Read(Core::Container::TSpan<u8>(bytes), decoded)) {
            REQUIRE(false);
            return;
        }
        REQUIRE_EQ(decoded.GetWidth(), image.GetWidth());
        REQUIRE_EQ(decoded.GetHeight(), image.GetHeight());
        REQUIRE_EQ(decoded.GetFormat(), EImageFormat::RGBA8);

        const u8* srcData = image.GetData();
        const u8* dstData = decoded.GetData();
        const auto bytesPerPixel = GetBytesPerPixel(image.GetFormat());
        const auto pixelCount    = image.GetWidth() * image.GetHeight();

        for (u32 i = 0; i < pixelCount; ++i) {
            const usize offset = static_cast<usize>(i) * bytesPerPixel;
            REQUIRE_CLOSE(srcData[offset + 0], dstData[offset + 0], 25.0);
            REQUIRE_CLOSE(srcData[offset + 1], dstData[offset + 1], 25.0);
            REQUIRE_CLOSE(srcData[offset + 2], dstData[offset + 2], 25.0);
            REQUIRE_EQ(dstData[offset + 3], 255);
        }
    }

    TEST_CASE("Imaging HSV disk saved and reloaded") {
        const u32 size = 256;
        FImage    image = MakeHsvDiskImage(size);
        if (!image.IsValid()) {
            REQUIRE(false);
            return;
        }

        std::error_code ec;
        const auto outputDir = std::filesystem::current_path() / "ImagingTestOutputs";
        std::filesystem::create_directories(outputDir, ec);
        if (ec) {
            REQUIRE(false);
            return;
        }

        const auto pngPath = outputDir / "hsv_disk.png";
        const auto jpgPath = outputDir / "hsv_disk.jpg";

        FPngImageWriter pngWriter;
        TVector<u8>     pngBytes;
        if (!pngWriter.Write(image.View(), pngBytes)) {
            REQUIRE(false);
            return;
        }
        if (!WriteFileBytes(pngPath, pngBytes)) {
            REQUIRE(false);
            return;
        }

        FJpegImageWriter jpegWriter(90);
        TVector<u8>      jpegBytes;
        if (!jpegWriter.Write(image.View(), jpegBytes)) {
            REQUIRE(false);
            return;
        }
        if (!WriteFileBytes(jpgPath, jpegBytes)) {
            REQUIRE(false);
            return;
        }

        TVector<u8> pngFileBytes;
        if (!ReadFileBytes(pngPath, pngFileBytes)) {
            REQUIRE(false);
            return;
        }
        FPngImageReader pngReader;
        FImage          pngImage;
        if (!pngReader.Read(Core::Container::TSpan<u8>(pngFileBytes), pngImage)) {
            REQUIRE(false);
            return;
        }
        REQUIRE_EQ(pngImage.GetWidth(), size);
        REQUIRE_EQ(pngImage.GetHeight(), size);
        REQUIRE_EQ(pngImage.GetFormat(), EImageFormat::RGBA8);

        TVector<u8> jpegFileBytes;
        if (!ReadFileBytes(jpgPath, jpegFileBytes)) {
            REQUIRE(false);
            return;
        }
        FJpegImageReader jpegReader;
        FImage           jpegImage;
        if (!jpegReader.Read(Core::Container::TSpan<u8>(jpegFileBytes), jpegImage)) {
            REQUIRE(false);
            return;
        }
        REQUIRE_EQ(jpegImage.GetWidth(), size);
        REQUIRE_EQ(jpegImage.GetHeight(), size);
        REQUIRE_EQ(jpegImage.GetFormat(), EImageFormat::RGBA8);
    }
#else
    TEST_CASE("Imaging PNG roundtrip (unsupported platform)") {
        REQUIRE(true);
    }

    TEST_CASE("Imaging JPEG roundtrip (unsupported platform)") {
        REQUIRE(true);
    }

    TEST_CASE("Imaging HSV disk saved and reloaded (unsupported platform)") {
        REQUIRE(true);
    }
#endif

} // namespace AltinaEngine::Imaging
