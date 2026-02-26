#include "Importers/EnvMap/EnvMapImporter.h"

#include "Asset/AssetBinary.h"
#include "Asset/Texture2DLoader.h"
#include "AssetToolIO.h"
#include "Utility/Json.h"
#include "Utility/Uuid.h"

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4242) // int -> u16 (tinyexr internal tables)
    #pragma warning(disable : 4245) // signed/unsigned mismatch (size_t returns)
    #pragma warning(disable : 4702) // unreachable code (tinyexr internal)
#endif
#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <sstream>
#include <string_view>

using AltinaEngine::Move;
namespace AltinaEngine::Tools::AssetPipeline {
    namespace {
        using Core::Container::FNativeString;
        using Core::Container::FNativeStringView;
        using Core::Utility::Json::EJsonType;
        using Core::Utility::Json::FindObjectValueInsensitive;
        using Core::Utility::Json::FJsonDocument;
        using Core::Utility::Json::FJsonValue;
        using Core::Utility::Json::GetBoolValue;
        using Core::Utility::Json::GetNumberValue;

        constexpr float kPi    = 3.14159265358979323846f;
        constexpr float kTwoPi = 6.28318530717958647692f;

        struct FVector3 {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
        };

        [[nodiscard]] auto MakeVec3(float x, float y, float z) -> FVector3 { return { x, y, z }; }
        [[nodiscard]] auto Add(const FVector3& a, const FVector3& b) -> FVector3 {
            return { a.x + b.x, a.y + b.y, a.z + b.z };
        }
        [[nodiscard]] auto Sub(const FVector3& a, const FVector3& b) -> FVector3 {
            return { a.x - b.x, a.y - b.y, a.z - b.z };
        }
        [[nodiscard]] auto Mul(const FVector3& a, float s) -> FVector3 {
            return { a.x * s, a.y * s, a.z * s };
        }
        [[nodiscard]] auto Div(const FVector3& a, float s) -> FVector3 {
            const float inv = (s != 0.0f) ? (1.0f / s) : 0.0f;
            return Mul(a, inv);
        }
        [[nodiscard]] auto Dot(const FVector3& a, const FVector3& b) -> float {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }
        [[nodiscard]] auto Cross(const FVector3& a, const FVector3& b) -> FVector3 {
            return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
        }
        [[nodiscard]] auto Length(const FVector3& v) -> float { return std::sqrt(Dot(v, v)); }
        [[nodiscard]] auto Normalize(const FVector3& v) -> FVector3 {
            const float len = Length(v);
            return (len > 0.0f) ? Div(v, len) : FVector3{};
        }
        [[nodiscard]] auto Clamp01(float v) -> float { return std::min(1.0f, std::max(0.0f, v)); }

        struct FHDRImage {
            u32                Width  = 0;
            u32                Height = 0;
            std::vector<float> Rgb; // Width * Height * 3, row-major, top-down

            [[nodiscard]] auto IsValid() const noexcept -> bool {
                return Width > 0U && Height > 0U
                    && Rgb.size() == static_cast<size_t>(Width) * static_cast<size_t>(Height) * 3U;
            }
        };

        [[nodiscard]] auto FloatToHalfBits(float value) -> u16 {
            // IEEE 754 float -> half conversion. Good enough for HDR environment maps.
            u32 bits = 0;
            static_assert(sizeof(bits) == sizeof(value));
            std::memcpy(&bits, &value, sizeof(bits));

            const u32 sign = (bits >> 16) & 0x8000u;
            const u32 mant = bits & 0x007FFFFFu;
            const i32 exp  = static_cast<i32>((bits >> 23) & 0xFFu) - 127;

            if (exp > 15) {
                // Overflow -> Inf.
                return static_cast<u16>(sign | 0x7C00u);
            }
            if (exp <= -15) {
                // Subnormal or zero.
                if (exp < -24) {
                    return static_cast<u16>(sign);
                }
                const u32 shifted = (mant | 0x00800000u) >> static_cast<u32>(-exp - 1);
                // Round to nearest.
                const u32 halfMant = (shifted + 0x00001000u) >> 13;
                return static_cast<u16>(sign | halfMant);
            }

            const u32 halfExp  = static_cast<u32>(exp + 15);
            const u32 halfMant = (mant + 0x00001000u) >> 13; // round
            return static_cast<u16>(sign | (halfExp << 10) | (halfMant & 0x03FFu));
        }

        auto DecodeOpenExr(const std::vector<u8>& sourceBytes, FHDRImage& outImage,
            std::string& outError) -> bool {
            outImage = {};
            outError.clear();

            if (sourceBytes.empty()) {
                outError = "Empty EXR bytes.";
                return false;
            }

            float*      rgba   = nullptr;
            int         width  = 0;
            int         height = 0;
            const char* err    = nullptr;
            const int   ret    = LoadEXRFromMemory(
                &rgba, &width, &height, sourceBytes.data(), sourceBytes.size(), &err);
            if (ret != TINYEXR_SUCCESS) {
                if (err != nullptr) {
                    outError = err;
                    FreeEXRErrorMessage(err);
                } else {
                    outError = "LoadEXRFromMemory failed.";
                }
                return false;
            }

            if (rgba == nullptr || width <= 0 || height <= 0) {
                if (rgba != nullptr) {
                    free(rgba); // NOLINT(*-no-malloc)
                }
                outError = "Invalid EXR decode output.";
                return false;
            }

            outImage.Width  = static_cast<u32>(width);
            outImage.Height = static_cast<u32>(height);
            outImage.Rgb.resize(static_cast<size_t>(outImage.Width) * outImage.Height * 3U);

            const size_t pixelCount =
                static_cast<size_t>(outImage.Width) * static_cast<size_t>(outImage.Height);
            for (size_t i = 0; i < pixelCount; ++i) {
                outImage.Rgb[i * 3U + 0U] = rgba[i * 4U + 0U];
                outImage.Rgb[i * 3U + 1U] = rgba[i * 4U + 1U];
                outImage.Rgb[i * 3U + 2U] = rgba[i * 4U + 2U];
            }

            free(rgba); // NOLINT(*-no-malloc)
            return outImage.IsValid();
        }

        [[nodiscard]] auto GetPixel(const FHDRImage& image, u32 x, u32 y) -> FVector3 {
            const u32    ix = (x >= image.Width) ? (image.Width - 1U) : x;
            const u32    iy = (y >= image.Height) ? (image.Height - 1U) : y;
            const size_t index =
                (static_cast<size_t>(iy) * static_cast<size_t>(image.Width) + ix) * 3U;
            return MakeVec3(image.Rgb[index + 0], image.Rgb[index + 1], image.Rgb[index + 2]);
        }

        [[nodiscard]] auto Lerp(const FVector3& a, const FVector3& b, float t) -> FVector3 {
            return Add(Mul(a, 1.0f - t), Mul(b, t));
        }

        [[nodiscard]] auto SampleEquirectBilinear(const FHDRImage& image, float u, float v)
            -> FVector3 {
            if (!image.IsValid()) {
                return {};
            }

            const float uu = u - std::floor(u); // wrap
            const float vv = Clamp01(v);        // clamp

            const float x = uu * static_cast<float>(image.Width);
            const float y = vv * static_cast<float>(image.Height);

            const int   x0 = static_cast<int>(std::floor(x)) % static_cast<int>(image.Width);
            const int   y0 = static_cast<int>(std::floor(y));
            const int   x1 = (x0 + 1) % static_cast<int>(image.Width);
            const int   y1 = std::min(static_cast<int>(image.Height) - 1, y0 + 1);
            const float tx = x - std::floor(x);
            const float ty = y - std::floor(y);

            const auto  c00 = GetPixel(image, static_cast<u32>(x0), static_cast<u32>(y0));
            const auto  c10 = GetPixel(image, static_cast<u32>(x1), static_cast<u32>(y0));
            const auto  c01 = GetPixel(image, static_cast<u32>(x0), static_cast<u32>(y1));
            const auto  c11 = GetPixel(image, static_cast<u32>(x1), static_cast<u32>(y1));

            const auto  cx0 = Lerp(c00, c10, tx);
            const auto  cx1 = Lerp(c01, c11, tx);
            return Lerp(cx0, cx1, ty);
        }

        [[nodiscard]] auto DirToEquirectUv(const FVector3& dir) -> std::pair<float, float> {
            const FVector3 d = Normalize(dir);
            const float    u = (std::atan2(d.z, d.x) / kTwoPi) + 0.5f;
            const float    v = 0.5f - (std::asin(std::min(1.0f, std::max(-1.0f, d.y))) / kPi);
            return { u, v };
        }

        enum class ECubeFace : u32 {
            PX = 0,
            NX = 1,
            PY = 2,
            NY = 3,
            PZ = 4,
            NZ = 5,
        };

        [[nodiscard]] auto FaceTexelToDir(ECubeFace face, u32 x, u32 y, u32 size) -> FVector3 {
            const float a =
                (2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(size)) - 1.0f;
            const float b =
                (2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(size)) - 1.0f;

            switch (face) {
                case ECubeFace::PX:
                    return Normalize(MakeVec3(1.0f, -b, -a));
                case ECubeFace::NX:
                    return Normalize(MakeVec3(-1.0f, -b, a));
                case ECubeFace::PY:
                    return Normalize(MakeVec3(a, 1.0f, b));
                case ECubeFace::NY:
                    return Normalize(MakeVec3(a, -1.0f, -b));
                case ECubeFace::PZ:
                    return Normalize(MakeVec3(a, -b, 1.0f));
                case ECubeFace::NZ:
                    return Normalize(MakeVec3(-a, -b, -1.0f));
                default:
                    return {};
            }
        }

        struct FEnvMapCookOptions {
            u32  SkyboxSize         = 256;
            u32  IrradianceSize     = 32;
            u32  SpecularSize       = 128;
            u32  BrdfLutSize        = 256;
            u32  DiffuseSamples     = 64;
            u32  SpecularSamples    = 256;
            u32  BrdfSamples        = 256;
            bool WriteBaseEquirect  = true;
            bool GenerateSkybox     = true;
            bool GenerateIrradiance = true;
            bool GenerateSpecular   = true;
            bool GenerateBrdfLut    = true;
        };

        [[nodiscard]] auto IsPow2(u32 v) noexcept -> bool {
            return v != 0U && (v & (v - 1U)) == 0U;
        }
        [[nodiscard]] auto FloorLog2(u32 v) noexcept -> u32 {
            u32 r = 0U;
            while (v > 1U) {
                v >>= 1U;
                ++r;
            }
            return r;
        }
        [[nodiscard]] auto FullMipCount(u32 size) noexcept -> u32 {
            return (size > 0U) ? (FloorLog2(size) + 1U) : 0U;
        }

        auto LoadEnvMapCookOptions(const std::filesystem::path& sourcePath,
            FEnvMapCookOptions& outOptions, std::string& outError) -> bool {
            outOptions = {};
            outError.clear();

            std::filesystem::path metaPath = sourcePath;
            metaPath += ".meta";

            std::error_code ec;
            if (!std::filesystem::exists(metaPath, ec)) {
                return true;
            }

            std::string metaText;
            if (!ReadFileText(metaPath, metaText)) {
                return true;
            }

            FNativeString native;
            native.Append(metaText.c_str(), metaText.size());
            const FNativeStringView view(native.GetData(), native.Length());

            FJsonDocument           document;
            if (!document.Parse(view)) {
                outError = "Invalid .meta JSON.";
                return false;
            }

            const FJsonValue* root = document.GetRoot();
            if (root == nullptr || root->Type != EJsonType::Object) {
                outError = "Invalid .meta JSON root.";
                return false;
            }

            auto ReadU32 = [&](const char* key, u32& outValue) -> bool {
                const FJsonValue* v = FindObjectValueInsensitive(*root, key);
                if (v == nullptr) {
                    return true;
                }
                double number = 0.0;
                if (!GetNumberValue(v, number) || number < 0.0
                    || number > static_cast<double>(std::numeric_limits<u32>::max())) {
                    outError = std::string("Invalid ") + key + " in .meta (expected number).";
                    return false;
                }
                outValue = static_cast<u32>(number);
                return true;
            };

            auto ReadBool = [&](const char* key, bool& outValue) -> bool {
                const FJsonValue* v = FindObjectValueInsensitive(*root, key);
                if (v == nullptr) {
                    return true;
                }
                bool flag = false;
                if (!GetBoolValue(v, flag)) {
                    outError = std::string("Invalid ") + key + " in .meta (expected boolean).";
                    return false;
                }
                outValue = flag;
                return true;
            };

            if (!ReadU32("EnvMapSkyboxSize", outOptions.SkyboxSize)) {
                return false;
            }
            if (!ReadU32("EnvMapIrradianceSize", outOptions.IrradianceSize)) {
                return false;
            }
            if (!ReadU32("EnvMapSpecularSize", outOptions.SpecularSize)) {
                return false;
            }
            if (!ReadU32("EnvMapBrdfLutSize", outOptions.BrdfLutSize)) {
                return false;
            }
            if (!ReadU32("EnvMapDiffuseSamples", outOptions.DiffuseSamples)) {
                return false;
            }
            if (!ReadU32("EnvMapSpecularSamples", outOptions.SpecularSamples)) {
                return false;
            }
            if (!ReadU32("EnvMapBrdfSamples", outOptions.BrdfSamples)) {
                return false;
            }
            if (!ReadBool("EnvMapWriteBaseEquirect", outOptions.WriteBaseEquirect)) {
                return false;
            }
            if (!ReadBool("EnvMapGenerateSkybox", outOptions.GenerateSkybox)) {
                return false;
            }
            if (!ReadBool("EnvMapGenerateIrradiance", outOptions.GenerateIrradiance)) {
                return false;
            }
            if (!ReadBool("EnvMapGenerateSpecular", outOptions.GenerateSpecular)) {
                return false;
            }
            if (!ReadBool("EnvMapGenerateBrdfLut", outOptions.GenerateBrdfLut)) {
                return false;
            }

            auto ClampSize = [](u32& v, u32 minV, u32 maxV) {
                v = std::min(maxV, std::max(minV, v));
            };
            ClampSize(outOptions.SkyboxSize, 4, 2048);
            ClampSize(outOptions.IrradianceSize, 4, 512);
            ClampSize(outOptions.SpecularSize, 4, 2048);
            ClampSize(outOptions.BrdfLutSize, 4, 1024);

            auto ClampSamples = [](u32& v, u32 minV, u32 maxV) {
                v = std::min(maxV, std::max(minV, v));
            };
            ClampSamples(outOptions.DiffuseSamples, 1, 4096);
            ClampSamples(outOptions.SpecularSamples, 1, 8192);
            ClampSamples(outOptions.BrdfSamples, 1, 8192);

            if (!IsPow2(outOptions.SkyboxSize) || !IsPow2(outOptions.IrradianceSize)
                || !IsPow2(outOptions.SpecularSize) || !IsPow2(outOptions.BrdfLutSize)) {
                outError = "EnvMap*Size must be power-of-two.";
                return false;
            }

            return true;
        }

        // FNV-based derived UUID (same scheme as AssimpModelImporter).
        constexpr u64 kFnvOffset = 14695981039346656037ULL;
        constexpr u64 kFnvPrime  = 1099511628211ULL;
        auto          HashBytes(u64 hash, const void* data, size_t size) -> u64 {
            const auto* bytes = static_cast<const u8*>(data);
            for (size_t i = 0; i < size; ++i) {
                hash ^= static_cast<u64>(bytes[i]);
                hash *= kFnvPrime;
            }
            return hash;
        }
        auto HashString(u64 hash, const std::string& text) -> u64 {
            return HashBytes(hash, text.data(), text.size());
        }
        auto MakeDerivedUuid(const FUuid& base, const std::string& salt) -> FUuid {
            u64 h1 = HashBytes(kFnvOffset, base.Data(), FUuid::kByteCount);
            h1     = HashString(h1, salt);
            u64 h2 = HashString(kFnvOffset, salt);
            h2     = HashBytes(h2, base.Data(), FUuid::kByteCount);

            FUuid::FBytes bytes{};
            for (u32 i = 0; i < 8U; ++i) {
                bytes[i]     = static_cast<u8>((h1 >> (i * 8U)) & 0xFFU);
                bytes[i + 8] = static_cast<u8>((h2 >> (i * 8U)) & 0xFFU);
            }
            return FUuid(bytes);
        }

        struct FCursor {
            const std::vector<u8>* Data   = nullptr;
            size_t                 Offset = 0;

            [[nodiscard]] auto     Remaining() const noexcept -> size_t {
                if (Data == nullptr || Offset > Data->size()) {
                    return 0U;
                }
                return Data->size() - Offset;
            }
        };

        auto ReadByte(FCursor& cursor, u8& out) -> bool {
            if (cursor.Data == nullptr || cursor.Offset >= cursor.Data->size()) {
                return false;
            }
            out = (*cursor.Data)[cursor.Offset++];
            return true;
        }

        auto ReadBytes(FCursor& cursor, void* outBuffer, size_t size) -> bool {
            if (outBuffer == nullptr || size == 0U) {
                return false;
            }
            if (cursor.Data == nullptr || cursor.Offset + size > cursor.Data->size()) {
                return false;
            }
            std::memcpy(outBuffer, cursor.Data->data() + cursor.Offset, size);
            cursor.Offset += size;
            return true;
        }

        [[nodiscard]] auto DecodeRgbeToFloat3(u8 r, u8 g, u8 b, u8 e) -> FVector3 {
            if (e == 0U) {
                return {};
            }
            const int   exponent = static_cast<int>(e) - (128 + 8);
            const float scale    = std::ldexp(1.0f, exponent);
            return MakeVec3(static_cast<float>(r) * scale, static_cast<float>(g) * scale,
                static_cast<float>(b) * scale);
        }

        auto EncodeFloat3ToRgbe(const FVector3& c, u8& outR, u8& outG, u8& outB, u8& outE) -> void {
            const float r = std::max(0.0f, c.x);
            const float g = std::max(0.0f, c.y);
            const float b = std::max(0.0f, c.z);
            const float v = std::max(r, std::max(g, b));
            if (v < 1e-32f) {
                outR = outG = outB = outE = 0U;
                return;
            }

            int         e     = 0;
            const float m     = std::frexp(v, &e); // v = m * 2^e
            const float scale = (m * 256.0f) / v;

            const int   ri = static_cast<int>(r * scale);
            const int   gi = static_cast<int>(g * scale);
            const int   bi = static_cast<int>(b * scale);

            outR = static_cast<u8>(std::min(255, std::max(0, ri)));
            outG = static_cast<u8>(std::min(255, std::max(0, gi)));
            outB = static_cast<u8>(std::min(255, std::max(0, bi)));
            outE = static_cast<u8>(std::min(255, std::max(0, e + 128)));
        }

        auto ParseHdrHeaderAndCursor(const std::vector<u8>& sourceBytes, u32& outWidth,
            u32& outHeight, size_t& outDataOffset, std::string& outError) -> bool {
            outWidth      = 0;
            outHeight     = 0;
            outDataOffset = 0;
            outError.clear();

            // Header is ASCII; treat bytes as a string for line parsing.
            std::string text(reinterpret_cast<const char*>(sourceBytes.data()),
                reinterpret_cast<const char*>(sourceBytes.data() + sourceBytes.size()));

            size_t      pos      = 0;
            auto        ReadLine = [&](std::string& outLine) -> bool {
                if (pos >= text.size()) {
                    return false;
                }
                const size_t lineEnd = text.find('\n', pos);
                if (lineEnd == std::string::npos) {
                    outLine = text.substr(pos);
                    pos     = text.size();
                    return true;
                }
                outLine = text.substr(pos, lineEnd - pos);
                pos     = lineEnd + 1;
                if (!outLine.empty() && outLine.back() == '\r') {
                    outLine.pop_back();
                }
                return true;
            };

            std::string line;
            bool        sawEmpty = false;
            while (ReadLine(line)) {
                if (line.empty()) {
                    sawEmpty = true;
                    break;
                }
            }
            if (!sawEmpty) {
                outError = "HDR header missing blank line.";
                return false;
            }

            if (!ReadLine(line)) {
                outError = "HDR missing resolution line.";
                return false;
            }

            // Expected: -Y <h> +X <w>
            std::istringstream res(line);
            std::string        yTag;
            std::string        xTag;
            int                h = 0;
            int                w = 0;
            res >> yTag >> h >> xTag >> w;
            if (yTag != "-Y" || xTag != "+X" || w <= 0 || h <= 0) {
                outError = "Unsupported HDR resolution/orientation (expected: -Y <h> +X <w>).";
                return false;
            }

            outWidth      = static_cast<u32>(w);
            outHeight     = static_cast<u32>(h);
            outDataOffset = pos;
            return true;
        }

        auto DecodeRadianceHdr(const std::vector<u8>& sourceBytes, FHDRImage& outImage,
            std::string& outError) -> bool {
            outImage = {};
            outError.clear();

            if (sourceBytes.size() < 16U) {
                outError = "HDR file too small.";
                return false;
            }

            u32    width      = 0;
            u32    height     = 0;
            size_t dataOffset = 0;
            if (!ParseHdrHeaderAndCursor(sourceBytes, width, height, dataOffset, outError)) {
                return false;
            }

            const u64 pixelCount = static_cast<u64>(width) * static_cast<u64>(height);
            if (pixelCount == 0U || pixelCount > (std::numeric_limits<size_t>::max() / 3U)) {
                outError = "HDR image too large.";
                return false;
            }

            outImage.Width  = width;
            outImage.Height = height;
            outImage.Rgb.resize(static_cast<size_t>(pixelCount) * 3U);

            FCursor cursor{};
            cursor.Data   = &sourceBytes;
            cursor.Offset = dataOffset;

            std::vector<u8> scanR(width);
            std::vector<u8> scanG(width);
            std::vector<u8> scanB(width);
            std::vector<u8> scanE(width);

            auto            DecodeRleChannel = [&](std::vector<u8>& outChannel) -> bool {
                size_t x = 0;
                while (x < outChannel.size()) {
                    u8 code = 0;
                    if (!ReadByte(cursor, code)) {
                        return false;
                    }
                    if (code > 128U) {
                        const size_t count = static_cast<size_t>(code - 128U);
                        u8           value = 0;
                        if (!ReadByte(cursor, value) || count == 0U) {
                            return false;
                        }
                        if (x + count > outChannel.size()) {
                            return false;
                        }
                        std::fill_n(outChannel.data() + x, count, value);
                        x += count;
                    } else {
                        const size_t count = static_cast<size_t>(code);
                        if (count == 0U || x + count > outChannel.size()) {
                            return false;
                        }
                        if (!ReadBytes(cursor, outChannel.data() + x, count)) {
                            return false;
                        }
                        x += count;
                    }
                }
                return true;
            };

            for (u32 y = 0; y < height; ++y) {
                u8 header[4]{};
                if (!ReadBytes(cursor, header, sizeof(header))) {
                    outError = "HDR truncated scanline.";
                    return false;
                }

                const bool rle = header[0] == 2U && header[1] == 2U && (header[2] & 0x80U) == 0U;
                if (!rle) {
                    // Old format (flat RGBE pixels).
                    cursor.Offset -= sizeof(header);
                    const size_t expectedBytes =
                        static_cast<size_t>(width) * static_cast<size_t>(height) * 4U;
                    if (cursor.Remaining() < expectedBytes) {
                        outError = "HDR truncated pixel data (old format).";
                        return false;
                    }

                    for (u32 yy = 0; yy < height; ++yy) {
                        for (u32 x = 0; x < width; ++x) {
                            u8 rgbe[4]{};
                            if (!ReadBytes(cursor, rgbe, sizeof(rgbe))) {
                                outError = "HDR truncated pixel data (old format).";
                                return false;
                            }
                            const auto   c = DecodeRgbeToFloat3(rgbe[0], rgbe[1], rgbe[2], rgbe[3]);
                            const size_t idx = (static_cast<size_t>(yy) * static_cast<size_t>(width)
                                                   + static_cast<size_t>(x))
                                * 3U;
                            outImage.Rgb[idx + 0] = c.x;
                            outImage.Rgb[idx + 1] = c.y;
                            outImage.Rgb[idx + 2] = c.z;
                        }
                    }
                    return true;
                }

                const u32 scanWidth = (static_cast<u32>(header[2]) << 8U) | header[3];
                if (scanWidth != width) {
                    outError = "HDR scanline width mismatch.";
                    return false;
                }

                if (!DecodeRleChannel(scanR) || !DecodeRleChannel(scanG) || !DecodeRleChannel(scanB)
                    || !DecodeRleChannel(scanE)) {
                    outError = "HDR RLE decode failed.";
                    return false;
                }

                for (u32 x = 0; x < width; ++x) {
                    const auto   c   = DecodeRgbeToFloat3(scanR[x], scanG[x], scanB[x], scanE[x]);
                    const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width)
                                           + static_cast<size_t>(x))
                        * 3U;
                    outImage.Rgb[idx + 0] = c.x;
                    outImage.Rgb[idx + 1] = c.y;
                    outImage.Rgb[idx + 2] = c.z;
                }
            }

            return true;
        }

        auto DownsampleBox2x(const std::vector<float>& srcRgb, u32 srcW, u32 srcH,
            std::vector<float>& outRgb, u32& outW, u32& outH) -> bool {
            if (srcW == 0U || srcH == 0U
                || srcRgb.size() != static_cast<size_t>(srcW) * static_cast<size_t>(srcH) * 3U) {
                return false;
            }
            outW = (srcW > 1U) ? (srcW >> 1U) : 1U;
            outH = (srcH > 1U) ? (srcH >> 1U) : 1U;

            outRgb.assign(static_cast<size_t>(outW) * static_cast<size_t>(outH) * 3U, 0.0f);

            auto Fetch = [&](u32 fx, u32 fy) -> FVector3 {
                const size_t idx =
                    (static_cast<size_t>(fy) * static_cast<size_t>(srcW) + static_cast<size_t>(fx))
                    * 3U;
                return MakeVec3(srcRgb[idx + 0], srcRgb[idx + 1], srcRgb[idx + 2]);
            };

            for (u32 y = 0; y < outH; ++y) {
                for (u32 x = 0; x < outW; ++x) {
                    const u32      x0 = std::min(srcW - 1U, x * 2U + 0U);
                    const u32      x1 = std::min(srcW - 1U, x * 2U + 1U);
                    const u32      y0 = std::min(srcH - 1U, y * 2U + 0U);
                    const u32      y1 = std::min(srcH - 1U, y * 2U + 1U);

                    const FVector3 c = Mul(
                        Add(Add(Fetch(x0, y0), Fetch(x1, y0)), Add(Fetch(x0, y1), Fetch(x1, y1))),
                        0.25f);

                    const size_t o = (static_cast<size_t>(y) * static_cast<size_t>(outW)
                                         + static_cast<size_t>(x))
                        * 3U;
                    outRgb[o + 0] = c.x;
                    outRgb[o + 1] = c.y;
                    outRgb[o + 2] = c.z;
                }
            }
            return true;
        }

        auto BuildTexture2DRgbeBlobFromMipChain(u32 baseW, u32 baseH,
            const std::vector<std::vector<float>>& mipRgb, bool srgb, std::vector<u8>& outCooked,
            Asset::FTexture2DDesc& outDesc, std::string& outError) -> bool {
            outCooked.clear();
            outDesc = {};
            outError.clear();

            if (baseW == 0U || baseH == 0U || mipRgb.empty()) {
                outError = "Invalid mip chain.";
                return false;
            }

            const u32 mipCount = static_cast<u32>(mipRgb.size());
            u32       w        = baseW;
            u32       h        = baseH;

            u64       totalBytes = 0;
            for (u32 mip = 0; mip < mipCount; ++mip) {
                const size_t expectedFloats = static_cast<size_t>(w) * static_cast<size_t>(h) * 3U;
                if (mipRgb[mip].size() != expectedFloats) {
                    outError = "Mip chain size mismatch.";
                    return false;
                }
                const u64 mipBytes = static_cast<u64>(w) * static_cast<u64>(h) * 4U;
                if (totalBytes > std::numeric_limits<u64>::max() - mipBytes) {
                    outError = "Mip chain too large.";
                    return false;
                }
                totalBytes += mipBytes;

                w = (w > 1U) ? (w >> 1U) : 1U;
                h = (h > 1U) ? (h >> 1U) : 1U;
            }

            if (totalBytes > static_cast<u64>(std::numeric_limits<u32>::max())) {
                outError = "Cooked texture data too large.";
                return false;
            }

            Asset::FTexture2DBlobDesc blobDesc{};
            blobDesc.Width    = baseW;
            blobDesc.Height   = baseH;
            blobDesc.Format   = Asset::kTextureFormatRGBA8;
            blobDesc.MipCount = mipCount;
            blobDesc.RowPitch = baseW * 4U;

            Asset::FAssetBlobHeader header{};
            header.Type     = static_cast<u8>(Asset::EAssetType::Texture2D);
            header.Flags    = Asset::MakeAssetBlobFlags(srgb);
            header.DescSize = static_cast<u32>(sizeof(Asset::FTexture2DBlobDesc));
            header.DataSize = static_cast<u32>(totalBytes);

            const size_t totalSize = sizeof(Asset::FAssetBlobHeader)
                + sizeof(Asset::FTexture2DBlobDesc) + static_cast<size_t>(totalBytes);
            outCooked.resize(totalSize);

            u8* write = outCooked.data();
            std::memcpy(write, &header, sizeof(header));
            write += sizeof(header);
            std::memcpy(write, &blobDesc, sizeof(blobDesc));
            write += sizeof(blobDesc);

            w = baseW;
            h = baseH;
            for (u32 mip = 0; mip < mipCount; ++mip) {
                const auto& src = mipRgb[mip];
                for (u32 y = 0; y < h; ++y) {
                    for (u32 x = 0; x < w; ++x) {
                        const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(w)
                                               + static_cast<size_t>(x))
                            * 3U;
                        u8 R = 0, G = 0, B = 0, E = 0;
                        EncodeFloat3ToRgbe(
                            MakeVec3(src[idx + 0], src[idx + 1], src[idx + 2]), R, G, B, E);
                        *write++ = R;
                        *write++ = G;
                        *write++ = B;
                        *write++ = E;
                    }
                }
                w = (w > 1U) ? (w >> 1U) : 1U;
                h = (h > 1U) ? (h >> 1U) : 1U;
            }

            outDesc.Width    = blobDesc.Width;
            outDesc.Height   = blobDesc.Height;
            outDesc.Format   = blobDesc.Format;
            outDesc.MipCount = blobDesc.MipCount;
            outDesc.SRGB     = srgb;
            return true;
        }

        auto BuildTexture2DBlobFromRgba8(u32 width, u32 height, const std::vector<u8>& rgba,
            bool srgb, std::vector<u8>& outCooked, Asset::FTexture2DDesc& outDesc,
            std::string& outError) -> bool {
            outCooked.clear();
            outDesc = {};
            outError.clear();

            if (width == 0U || height == 0U
                || rgba.size() != static_cast<size_t>(width) * static_cast<size_t>(height) * 4U) {
                outError = "Invalid RGBA8 data.";
                return false;
            }

            Asset::FTexture2DBlobDesc blobDesc{};
            blobDesc.Width    = width;
            blobDesc.Height   = height;
            blobDesc.Format   = Asset::kTextureFormatRGBA8;
            blobDesc.MipCount = 1;
            blobDesc.RowPitch = width * 4U;

            Asset::FAssetBlobHeader header{};
            header.Type     = static_cast<u8>(Asset::EAssetType::Texture2D);
            header.Flags    = Asset::MakeAssetBlobFlags(srgb);
            header.DescSize = static_cast<u32>(sizeof(Asset::FTexture2DBlobDesc));
            header.DataSize = static_cast<u32>(rgba.size());

            outCooked.resize(sizeof(header) + sizeof(blobDesc) + rgba.size());
            u8* write = outCooked.data();
            std::memcpy(write, &header, sizeof(header));
            write += sizeof(header);
            std::memcpy(write, &blobDesc, sizeof(blobDesc));
            write += sizeof(blobDesc);
            std::memcpy(write, rgba.data(), rgba.size());

            outDesc.Width    = blobDesc.Width;
            outDesc.Height   = blobDesc.Height;
            outDesc.Format   = blobDesc.Format;
            outDesc.MipCount = blobDesc.MipCount;
            outDesc.SRGB     = srgb;
            return true;
        }

        auto BuildCubeMapRgba16fBlobFromMipChains(u32             size,
            const std::array<std::vector<std::vector<float>>, 6>& faceMipChains,
            std::vector<u8>& outCooked, Asset::FCubeMapDesc& outDesc, std::string& outError)
            -> bool {
            outCooked.clear();
            outDesc = {};
            outError.clear();

            if (size == 0U) {
                outError = "Invalid cube map size.";
                return false;
            }

            const u32 mipCount = FullMipCount(size);
            const u32 format   = Asset::kTextureFormatRGBA16F;
            const u32 bpp      = Asset::GetTextureBytesPerPixel(format);
            if (bpp == 0U) {
                outError = "Unsupported cube map format.";
                return false;
            }

            u64 totalBytes = 0ULL;
            u32 mipSize    = size;
            for (u32 mip = 0U; mip < mipCount; ++mip) {
                const u64 faceBytes =
                    static_cast<u64>(mipSize) * static_cast<u64>(mipSize) * static_cast<u64>(bpp);
                totalBytes += faceBytes * 6ULL;
                mipSize = (mipSize > 1U) ? (mipSize >> 1U) : 1U;
            }

            if (totalBytes > static_cast<u64>(std::numeric_limits<u32>::max())) {
                outError = "Cooked cubemap data too large.";
                return false;
            }

            Asset::FCubeMapBlobDesc blobDesc{};
            blobDesc.Size     = size;
            blobDesc.Format   = format;
            blobDesc.MipCount = mipCount;
            blobDesc.RowPitch = size * bpp;

            Asset::FAssetBlobHeader header{};
            header.Type     = static_cast<u8>(Asset::EAssetType::CubeMap);
            header.Flags    = Asset::MakeAssetBlobFlags(false);
            header.DescSize = static_cast<u32>(sizeof(Asset::FCubeMapBlobDesc));
            header.DataSize = static_cast<u32>(totalBytes);

            const size_t totalSize =
                sizeof(header) + sizeof(blobDesc) + static_cast<size_t>(totalBytes);
            outCooked.resize(totalSize);

            u8* write = outCooked.data();
            std::memcpy(write, &header, sizeof(header));
            write += sizeof(header);
            std::memcpy(write, &blobDesc, sizeof(blobDesc));
            write += sizeof(blobDesc);

            for (u32 mip = 0U; mip < mipCount; ++mip) {
                const u32    mipDim = std::max(1U, size >> mip);
                const size_t expectedFloats =
                    static_cast<size_t>(mipDim) * static_cast<size_t>(mipDim) * 3U;

                for (u32 face = 0U; face < 6U; ++face) {
                    if (faceMipChains[face].size() != static_cast<size_t>(mipCount)
                        || faceMipChains[face][mip].size() != expectedFloats) {
                        outError = "Cubemap mip chain size mismatch.";
                        return false;
                    }

                    const auto&  src = faceMipChains[face][mip];
                    const size_t pixelCount =
                        static_cast<size_t>(mipDim) * static_cast<size_t>(mipDim);

                    for (size_t i = 0U; i < pixelCount; ++i) {
                        const float r = src[i * 3U + 0U];
                        const float g = src[i * 3U + 1U];
                        const float b = src[i * 3U + 2U];

                        const u16   half[4] = { FloatToHalfBits(r), FloatToHalfBits(g),
                              FloatToHalfBits(b), FloatToHalfBits(1.0f) };
                        std::memcpy(write, half, sizeof(half));
                        write += sizeof(half);
                    }
                }
            }

            outDesc.Size     = blobDesc.Size;
            outDesc.MipCount = blobDesc.MipCount;
            outDesc.Format   = blobDesc.Format;
            outDesc.SRGB     = false;
            return true;
        }

        auto ValidateCookedTexture2D(
            const std::vector<u8>& cookedBytes, const Asset::FTexture2DDesc& expectedDesc) -> bool {
            class FMemoryAssetStream final : public Asset::IAssetStream {
            public:
                explicit FMemoryAssetStream(const std::vector<u8>& data) : mData(data) {}

                [[nodiscard]] auto Size() const noexcept -> usize override {
                    return static_cast<usize>(mData.size());
                }
                [[nodiscard]] auto Tell() const noexcept -> usize override { return mOffset; }
                void               Seek(usize offset) noexcept override {
                    mOffset = (offset > Size()) ? Size() : offset;
                }
                auto Read(void* outBuffer, usize bytesToRead) -> usize override {
                    if (outBuffer == nullptr || bytesToRead == 0U) {
                        return 0U;
                    }
                    const usize remaining = Size() - mOffset;
                    const usize toRead    = (bytesToRead > remaining) ? remaining : bytesToRead;
                    if (toRead == 0U) {
                        return 0U;
                    }
                    std::memcpy(outBuffer, mData.data() + static_cast<size_t>(mOffset),
                        static_cast<size_t>(toRead));
                    mOffset += toRead;
                    return toRead;
                }

            private:
                const std::vector<u8>& mData;
                usize                  mOffset = 0;
            };

            Asset::FAssetDesc desc{};
            desc.Texture = expectedDesc;
            FMemoryAssetStream      stream(cookedBytes);
            Asset::FTexture2DLoader loader;
            return static_cast<bool>(loader.Load(desc, stream));
        }

        [[nodiscard]] auto RadicalInverseVdC(u32 bits) -> float {
            bits = (bits << 16U) | (bits >> 16U);
            bits = ((bits & 0x55555555U) << 1U) | ((bits & 0xAAAAAAAAU) >> 1U);
            bits = ((bits & 0x33333333U) << 2U) | ((bits & 0xCCCCCCCCU) >> 2U);
            bits = ((bits & 0x0F0F0F0FU) << 4U) | ((bits & 0xF0F0F0F0U) >> 4U);
            bits = ((bits & 0x00FF00FFU) << 8U) | ((bits & 0xFF00FF00U) >> 8U);
            return static_cast<float>(bits) * 2.3283064365386963e-10f; // / 0x100000000
        }

        [[nodiscard]] auto Hammersley(u32 i, u32 n) -> std::pair<float, float> {
            const float x = (n > 0U) ? (static_cast<float>(i) / static_cast<float>(n)) : 0.0f;
            const float y = RadicalInverseVdC(i);
            return { x, y };
        }

        [[nodiscard]] auto BuildTangentBasis(const FVector3& n, FVector3& outT, FVector3& outB)
            -> void {
            const FVector3 up =
                (std::abs(n.z) < 0.999f) ? MakeVec3(0.0f, 0.0f, 1.0f) : MakeVec3(1.0f, 0.0f, 0.0f);
            outT = Normalize(Cross(up, n));
            outB = Cross(n, outT);
        }

        [[nodiscard]] auto CosineSampleHemisphere(float u1, float u2) -> FVector3 {
            const float r   = std::sqrt(u1);
            const float phi = kTwoPi * u2;
            const float x   = r * std::cos(phi);
            const float y   = r * std::sin(phi);
            const float z   = std::sqrt(std::max(0.0f, 1.0f - u1));
            return MakeVec3(x, y, z);
        }

        [[nodiscard]] auto ImportanceSampleGGX(
            float u1, float u2, const FVector3& n, float roughness) -> FVector3 {
            const float    a  = std::max(roughness * roughness, 0.001f);
            const float    a2 = a * a;

            const float    phi      = kTwoPi * u1;
            const float    cosTheta = std::sqrt((1.0f - u2) / (1.0f + (a2 - 1.0f) * u2));
            const float    sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

            const FVector3 hTangent =
                MakeVec3(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta);

            FVector3 t, b;
            BuildTangentBasis(n, t, b);
            const FVector3 hWorld =
                Add(Add(Mul(t, hTangent.x), Mul(b, hTangent.y)), Mul(n, hTangent.z));
            return Normalize(hWorld);
        }

        [[nodiscard]] auto D_GGX(float nDotH, float roughness) -> float {
            const float a     = std::max(roughness * roughness, 0.001f);
            const float a2    = a * a;
            const float denom = (nDotH * nDotH) * (a2 - 1.0f) + 1.0f;
            return a2 / (kPi * denom * denom);
        }

        [[nodiscard]] auto GeometrySchlickGGX(float nDotV, float roughness) -> float {
            const float r = roughness + 1.0f;
            const float k = (r * r) / 8.0f;
            return nDotV / (nDotV * (1.0f - k) + k);
        }

        [[nodiscard]] auto GeometrySmith(float nDotV, float nDotL, float roughness) -> float {
            const float ggx1 = GeometrySchlickGGX(nDotV, roughness);
            const float ggx2 = GeometrySchlickGGX(nDotL, roughness);
            return ggx1 * ggx2;
        }

        auto GenerateSkyboxFace(
            const FHDRImage& src, ECubeFace face, u32 size, std::vector<float>& outRgb) -> bool {
            outRgb.assign(static_cast<size_t>(size) * static_cast<size_t>(size) * 3U, 0.0f);
            for (u32 y = 0; y < size; ++y) {
                for (u32 x = 0; x < size; ++x) {
                    const FVector3 dir = FaceTexelToDir(face, x, y, size);
                    const auto [u, v]  = DirToEquirectUv(dir);
                    const FVector3 c   = SampleEquirectBilinear(src, u, v);
                    const size_t   idx = (static_cast<size_t>(y) * static_cast<size_t>(size)
                                           + static_cast<size_t>(x))
                        * 3U;
                    outRgb[idx + 0] = c.x;
                    outRgb[idx + 1] = c.y;
                    outRgb[idx + 2] = c.z;
                }
            }
            return true;
        }

        auto GenerateIrradianceFace(const FHDRImage& src, ECubeFace face, u32 size, u32 sampleCount,
            std::vector<float>& outRgb) -> bool {
            outRgb.assign(static_cast<size_t>(size) * static_cast<size_t>(size) * 3U, 0.0f);
            for (u32 y = 0; y < size; ++y) {
                for (u32 x = 0; x < size; ++x) {
                    const FVector3 n = FaceTexelToDir(face, x, y, size);
                    FVector3       t, b;
                    BuildTangentBasis(n, t, b);

                    FVector3 accum{};
                    for (u32 i = 0; i < sampleCount; ++i) {
                        const auto [u1, u2] = Hammersley(i, sampleCount);
                        const FVector3 h    = CosineSampleHemisphere(u1, u2); // local
                        const FVector3 l =
                            Normalize(Add(Add(Mul(t, h.x), Mul(b, h.y)), Mul(n, h.z)));
                        const auto [eu, ev] = DirToEquirectUv(l);
                        accum               = Add(accum, SampleEquirectBilinear(src, eu, ev));
                    }

                    const FVector3 irradiance = Div(accum, static_cast<float>(sampleCount));
                    const size_t   idx        = (static_cast<size_t>(y) * static_cast<size_t>(size)
                                           + static_cast<size_t>(x))
                        * 3U;
                    outRgb[idx + 0] = irradiance.x;
                    outRgb[idx + 1] = irradiance.y;
                    outRgb[idx + 2] = irradiance.z;
                }
            }
            return true;
        }

        auto GenerateSpecularPrefilterMip(const FHDRImage& src, ECubeFace face, u32 size,
            u32 sampleCount, float roughness, std::vector<float>& outRgb) -> bool {
            outRgb.assign(static_cast<size_t>(size) * static_cast<size_t>(size) * 3U, 0.0f);
            for (u32 y = 0; y < size; ++y) {
                for (u32 x = 0; x < size; ++x) {
                    const FVector3 n = FaceTexelToDir(face, x, y, size);
                    const FVector3 v = n;

                    FVector3       prefiltered{};
                    float          totalWeight = 0.0f;
                    for (u32 i = 0; i < sampleCount; ++i) {
                        const auto [u1, u2]  = Hammersley(i, sampleCount);
                        const FVector3 h     = ImportanceSampleGGX(u1, u2, n, roughness);
                        const float    vDotH = std::max(0.0f, Dot(v, h));
                        const FVector3 l     = Normalize(Sub(Mul(h, 2.0f * vDotH), v));

                        const float    nDotL = std::max(0.0f, Dot(n, l));
                        if (nDotL <= 0.0f) {
                            continue;
                        }

                        const float nDotH  = std::max(0.0f, Dot(n, h));
                        const float d      = D_GGX(nDotH, roughness);
                        const float pdf    = (d * nDotH / (4.0f * vDotH)) + 1e-6f;
                        const float weight = nDotL / pdf;

                        const auto [eu, ev] = DirToEquirectUv(l);
                        prefiltered =
                            Add(prefiltered, Mul(SampleEquirectBilinear(src, eu, ev), weight));
                        totalWeight += weight;
                    }

                    const FVector3 color =
                        (totalWeight > 0.0f) ? Div(prefiltered, totalWeight) : FVector3{};
                    const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(size)
                                           + static_cast<size_t>(x))
                        * 3U;
                    outRgb[idx + 0] = color.x;
                    outRgb[idx + 1] = color.y;
                    outRgb[idx + 2] = color.z;
                }
            }
            return true;
        }

        [[nodiscard]] auto IntegrateBrdf(float nDotV, float roughness, u32 sampleCount)
            -> std::pair<float, float> {
            const float    ndv = std::max(0.0f, std::min(1.0f, nDotV));
            const FVector3 v   = MakeVec3(std::sqrt(std::max(0.0f, 1.0f - ndv * ndv)), 0.0f, ndv);
            const FVector3 n   = MakeVec3(0.0f, 0.0f, 1.0f);

            float          a = 0.0f;
            float          b = 0.0f;
            for (u32 i = 0; i < sampleCount; ++i) {
                const auto [u1, u2]  = Hammersley(i, sampleCount);
                const FVector3 h     = ImportanceSampleGGX(u1, u2, n, roughness);
                const float    vDotH = std::max(0.0f, Dot(v, h));
                const FVector3 l     = Normalize(Sub(Mul(h, 2.0f * vDotH), v));

                const float    nDotL = std::max(0.0f, l.z);
                const float    nDotH = std::max(0.0f, h.z);
                if (nDotL <= 0.0f) {
                    continue;
                }

                const float g    = GeometrySmith(ndv, nDotL, roughness);
                const float gVis = (g * vDotH) / std::max(1e-6f, (nDotH * ndv));
                const float fc   = std::pow(1.0f - vDotH, 5.0f);
                a += (1.0f - fc) * gVis;
                b += fc * gVis;
            }

            const float inv = (sampleCount > 0U) ? (1.0f / static_cast<float>(sampleCount)) : 0.0f;
            return { Clamp01(a * inv), Clamp01(b * inv) };
        }

        auto BuildBrdfLutRgba8(u32 size, u32 sampleCount, std::vector<u8>& outRgba) -> bool {
            outRgba.assign(static_cast<size_t>(size) * static_cast<size_t>(size) * 4U, 0U);
            for (u32 y = 0; y < size; ++y) {
                const float roughness =
                    (size > 1U) ? (static_cast<float>(y) / static_cast<float>(size - 1U)) : 0.0f;
                for (u32 x = 0; x < size; ++x) {
                    const float nDotV = (size > 1U)
                        ? (static_cast<float>(x) / static_cast<float>(size - 1U))
                        : 0.0f;
                    const auto [a, b] = IntegrateBrdf(nDotV, roughness, sampleCount);
                    const size_t idx  = (static_cast<size_t>(y) * static_cast<size_t>(size)
                                           + static_cast<size_t>(x))
                        * 4U;
                    outRgba[idx + 0] =
                        static_cast<u8>(std::min(255.0f, std::max(0.0f, std::round(a * 255.0f))));
                    outRgba[idx + 1] =
                        static_cast<u8>(std::min(255.0f, std::max(0.0f, std::round(b * 255.0f))));
                    outRgba[idx + 2] = 0U;
                    outRgba[idx + 3] = 255U;
                }
            }
            return true;
        }

        auto MakeFaceVirtualPath(const std::string& baseVirtualPath, std::string_view product,
            std::string_view face) -> std::string {
            std::string out = baseVirtualPath;
            if (!out.empty() && out.back() != '/') {
                out.push_back('/');
            }
            out.append(product);
            out.push_back('/');
            out.append(face);
            return out;
        }

        auto AddGeneratedTexture(const Asset::FAssetHandle& baseHandle,
            const std::string& derivedVirtualPath, const std::vector<u8>& cookedBytes,
            const Asset::FTexture2DDesc& desc, std::vector<FGeneratedAsset>& outGenerated,
            std::string& outError) -> bool {
            if (!baseHandle.IsValid() || baseHandle.Type != Asset::EAssetType::Texture2D) {
                outError = "Invalid base handle.";
                return false;
            }
            if (derivedVirtualPath.empty()) {
                outError = "Derived virtual path is empty.";
                return false;
            }

            const FUuid         derivedUuid = MakeDerivedUuid(baseHandle.Uuid, derivedVirtualPath);
            Asset::FAssetHandle handle{};
            handle.Uuid = derivedUuid;
            handle.Type = Asset::EAssetType::Texture2D;

            FGeneratedAsset gen{};
            gen.Handle      = handle;
            gen.Type        = Asset::EAssetType::Texture2D;
            gen.VirtualPath = derivedVirtualPath;
            gen.CookedBytes = cookedBytes;
            gen.TextureDesc = desc;
            outGenerated.push_back(Move(gen));
            return true;
        }

        struct FCookExtrasV1 {
            u32 Magic           = 0x50414D45U; // "EMAP"
            u32 Version         = 1U;
            u32 SkyboxSize      = 0;
            u32 IrradianceSize  = 0;
            u32 SpecularSize    = 0;
            u32 BrdfLutSize     = 0;
            u32 DiffuseSamples  = 0;
            u32 SpecularSamples = 0;
            u32 BrdfSamples     = 0;
            u32 Flags = 0; // bit0 base, bit1 skybox, bit2 irradiance, bit3 specular, bit4 brdf
        };

        constexpr u32 kFlagBase = 1u << 0;
        constexpr u32 kFlagSky  = 1u << 1;
        constexpr u32 kFlagDiff = 1u << 2;
        constexpr u32 kFlagSpec = 1u << 3;
        constexpr u32 kFlagBrdf = 1u << 4;
    } // namespace

    auto CookEnvMapFromHdr(const std::filesystem::path& sourcePath,
        const std::vector<u8>& sourceBytes, const Asset::FAssetHandle& baseHandle,
        const std::string& baseVirtualPath, FEnvMapCookResult& outResult, std::string& outError)
        -> bool {
        outResult = {};
        outError.clear();

        if (!baseHandle.IsValid() || baseHandle.Type != Asset::EAssetType::Texture2D) {
            outError = "Base handle must be a valid Texture2D handle.";
            return false;
        }
        if (baseVirtualPath.empty()) {
            outError = "Base virtual path is empty.";
            return false;
        }

        FEnvMapCookOptions options{};
        std::string        optError;
        if (!LoadEnvMapCookOptions(sourcePath, options, optError)) {
            outError = optError;
            return false;
        }

        FHDRImage image;
        if (!DecodeRadianceHdr(sourceBytes, image, outError)) {
            return false;
        }

        // Base cooked asset: RGBE-encoded equirectangular. Stored as a regular Texture2D (RGBA8,
        // SRGB=false).
        {
            u32             baseW = 1;
            u32             baseH = 1;
            std::vector<u8> rgba;
            if (options.WriteBaseEquirect) {
                baseW = image.Width;
                baseH = image.Height;
                rgba.resize(static_cast<size_t>(baseW) * static_cast<size_t>(baseH) * 4U);
                for (u32 y = 0; y < baseH; ++y) {
                    for (u32 x = 0; x < baseW; ++x) {
                        const size_t srcIdx = (static_cast<size_t>(y) * static_cast<size_t>(baseW)
                                                  + static_cast<size_t>(x))
                            * 3U;
                        u8 R = 0, G = 0, B = 0, E = 0;
                        EncodeFloat3ToRgbe(MakeVec3(image.Rgb[srcIdx + 0], image.Rgb[srcIdx + 1],
                                               image.Rgb[srcIdx + 2]),
                            R, G, B, E);
                        const size_t dstIdx = (static_cast<size_t>(y) * static_cast<size_t>(baseW)
                                                  + static_cast<size_t>(x))
                            * 4U;
                        rgba[dstIdx + 0] = R;
                        rgba[dstIdx + 1] = G;
                        rgba[dstIdx + 2] = B;
                        rgba[dstIdx + 3] = E;
                    }
                }
            } else {
                // Keep the base asset cookable/parsable even when the equirect texture isn't
                // requested.
                rgba = { 0U, 0U, 0U, 0U };
            }

            std::vector<u8>       cooked;
            Asset::FTexture2DDesc desc{};
            std::string           blobError;
            if (!BuildTexture2DBlobFromRgba8(baseW, baseH, rgba, false, cooked, desc, blobError)) {
                outError = blobError;
                return false;
            }
            if (!ValidateCookedTexture2D(cooked, desc)) {
                outError = "Base cooked Texture2D failed to validate.";
                return false;
            }

            outResult.CookedBytes = Move(cooked);
            outResult.Desc        = desc;
        }

        static constexpr std::array<const char*, 6> kFaceNames = { "px", "nx", "py", "ny", "pz",
            "nz" };

        if (options.GenerateSkybox) {
            const u32 size     = options.SkyboxSize;
            const u32 mipCount = FullMipCount(size);
            for (u32 f = 0; f < 6U; ++f) {
                std::vector<std::vector<float>> mipChain;
                mipChain.resize(mipCount);

                if (!GenerateSkyboxFace(image, static_cast<ECubeFace>(f), size, mipChain[0])) {
                    outError = "Failed to generate skybox face.";
                    return false;
                }

                u32 w = size;
                u32 h = size;
                for (u32 mip = 1; mip < mipCount; ++mip) {
                    u32 nw = 0, nh = 0;
                    if (!DownsampleBox2x(mipChain[mip - 1], w, h, mipChain[mip], nw, nh)) {
                        outError = "Failed to downsample skybox mip.";
                        return false;
                    }
                    w = nw;
                    h = nh;
                }

                std::vector<u8>       cooked;
                Asset::FTexture2DDesc desc{};
                std::string           blobError;
                if (!BuildTexture2DRgbeBlobFromMipChain(
                        size, size, mipChain, false, cooked, desc, blobError)) {
                    outError = blobError;
                    return false;
                }
                if (!ValidateCookedTexture2D(cooked, desc)) {
                    outError = "Skybox face Texture2D failed to validate.";
                    return false;
                }

                const std::string vpath =
                    MakeFaceVirtualPath(baseVirtualPath, "skybox", kFaceNames[f]);
                if (!AddGeneratedTexture(
                        baseHandle, vpath, cooked, desc, outResult.Generated, outError)) {
                    return false;
                }
            }
        }

        if (options.GenerateIrradiance) {
            const u32 size = options.IrradianceSize;
            for (u32 f = 0; f < 6U; ++f) {
                std::vector<std::vector<float>> mipChain;
                mipChain.resize(1);

                if (!GenerateIrradianceFace(image, static_cast<ECubeFace>(f), size,
                        options.DiffuseSamples, mipChain[0])) {
                    outError = "Failed to generate irradiance face.";
                    return false;
                }

                std::vector<u8>       cooked;
                Asset::FTexture2DDesc desc{};
                std::string           blobError;
                if (!BuildTexture2DRgbeBlobFromMipChain(
                        size, size, mipChain, false, cooked, desc, blobError)) {
                    outError = blobError;
                    return false;
                }
                if (!ValidateCookedTexture2D(cooked, desc)) {
                    outError = "Irradiance face Texture2D failed to validate.";
                    return false;
                }

                const std::string vpath =
                    MakeFaceVirtualPath(baseVirtualPath, "irradiance", kFaceNames[f]);
                if (!AddGeneratedTexture(
                        baseHandle, vpath, cooked, desc, outResult.Generated, outError)) {
                    return false;
                }
            }
        }

        if (options.GenerateSpecular) {
            const u32 baseSize = options.SpecularSize;
            const u32 mipCount = FullMipCount(baseSize);
            for (u32 f = 0; f < 6U; ++f) {
                std::vector<std::vector<float>> mipChain;
                mipChain.resize(mipCount);

                for (u32 mip = 0; mip < mipCount; ++mip) {
                    const float roughness = (mipCount > 1U)
                        ? (static_cast<float>(mip) / static_cast<float>(mipCount - 1U))
                        : 0.0f;
                    const u32   mipSize   = std::max(1U, baseSize >> mip);
                    const float r         = std::max(roughness, 0.001f);

                    if (!GenerateSpecularPrefilterMip(image, static_cast<ECubeFace>(f), mipSize,
                            options.SpecularSamples, r, mipChain[mip])) {
                        outError = "Failed to generate specular prefilter mip.";
                        return false;
                    }
                }

                std::vector<u8>       cooked;
                Asset::FTexture2DDesc desc{};
                std::string           blobError;
                if (!BuildTexture2DRgbeBlobFromMipChain(
                        baseSize, baseSize, mipChain, false, cooked, desc, blobError)) {
                    outError = blobError;
                    return false;
                }
                if (!ValidateCookedTexture2D(cooked, desc)) {
                    outError = "Specular face Texture2D failed to validate.";
                    return false;
                }

                const std::string vpath =
                    MakeFaceVirtualPath(baseVirtualPath, "specular", kFaceNames[f]);
                if (!AddGeneratedTexture(
                        baseHandle, vpath, cooked, desc, outResult.Generated, outError)) {
                    return false;
                }
            }
        }

        if (options.GenerateBrdfLut) {
            std::vector<u8> rgba;
            if (!BuildBrdfLutRgba8(options.BrdfLutSize, options.BrdfSamples, rgba)) {
                outError = "Failed to build BRDF LUT.";
                return false;
            }

            std::vector<u8>       cooked;
            Asset::FTexture2DDesc desc{};
            std::string           blobError;
            if (!BuildTexture2DBlobFromRgba8(options.BrdfLutSize, options.BrdfLutSize, rgba, false,
                    cooked, desc, blobError)) {
                outError = blobError;
                return false;
            }
            if (!ValidateCookedTexture2D(cooked, desc)) {
                outError = "BRDF LUT Texture2D failed to validate.";
                return false;
            }

            std::string vpath = baseVirtualPath;
            if (!vpath.empty() && vpath.back() != '/') {
                vpath.push_back('/');
            }
            vpath.append("brdf_lut");

            if (!AddGeneratedTexture(
                    baseHandle, vpath, cooked, desc, outResult.Generated, outError)) {
                return false;
            }
        }

        FCookExtrasV1 extras{};
        extras.SkyboxSize      = options.SkyboxSize;
        extras.IrradianceSize  = options.IrradianceSize;
        extras.SpecularSize    = options.SpecularSize;
        extras.BrdfLutSize     = options.BrdfLutSize;
        extras.DiffuseSamples  = options.DiffuseSamples;
        extras.SpecularSamples = options.SpecularSamples;
        extras.BrdfSamples     = options.BrdfSamples;
        extras.Flags |= options.WriteBaseEquirect ? kFlagBase : 0U;
        extras.Flags |= options.GenerateSkybox ? kFlagSky : 0U;
        extras.Flags |= options.GenerateIrradiance ? kFlagDiff : 0U;
        extras.Flags |= options.GenerateSpecular ? kFlagSpec : 0U;
        extras.Flags |= options.GenerateBrdfLut ? kFlagBrdf : 0U;

        outResult.CookKeyExtras.resize(sizeof(extras));
        std::memcpy(outResult.CookKeyExtras.data(), &extras, sizeof(extras));

        return true;
    }

    auto CookSkyCubeFromExr(const std::filesystem::path& sourcePath,
        const std::vector<u8>& sourceBytes, FSkyCubeCookResult& outResult, std::string& outError)
        -> bool {
        outResult = {};
        outError.clear();

        FEnvMapCookOptions options{};
        std::string        optError;
        if (!LoadEnvMapCookOptions(sourcePath, options, optError)) {
            outError = optError;
            return false;
        }

        FHDRImage image{};
        if (!DecodeOpenExr(sourceBytes, image, outError)) {
            return false;
        }

        const u32 size = options.SkyboxSize;
        if (size == 0U) {
            outError = "EnvMapSkyboxSize must be > 0.";
            return false;
        }

        const u32                                      mipCount = FullMipCount(size);
        std::array<std::vector<std::vector<float>>, 6> faceMipChains;
        for (u32 f = 0U; f < 6U; ++f) {
            faceMipChains[f].resize(mipCount);

            if (!GenerateSkyboxFace(image, static_cast<ECubeFace>(f), size, faceMipChains[f][0])) {
                outError = "Failed to generate skybox face.";
                return false;
            }

            u32 w = size;
            u32 h = size;
            for (u32 mip = 1U; mip < mipCount; ++mip) {
                u32 nw = 0U, nh = 0U;
                if (!DownsampleBox2x(
                        faceMipChains[f][mip - 1U], w, h, faceMipChains[f][mip], nw, nh)) {
                    outError = "Failed to downsample skybox mip.";
                    return false;
                }
                w = nw;
                h = nh;
            }
        }

        std::vector<u8>     cooked;
        Asset::FCubeMapDesc desc{};
        std::string         cookError;
        if (!BuildCubeMapRgba16fBlobFromMipChains(size, faceMipChains, cooked, desc, cookError)) {
            outError = cookError;
            return false;
        }

        // Stable cook key extras: options relevant to cube generation.
        outResult.CookKeyExtras.resize(sizeof(u32));
        std::memcpy(outResult.CookKeyExtras.data(), &options.SkyboxSize, sizeof(u32));

        outResult.CookedBytes = Move(cooked);
        outResult.Desc        = desc;
        return true;
    }

    auto CookSkyCubeFromHdr(const std::filesystem::path& sourcePath,
        const std::vector<u8>& sourceBytes, FSkyCubeCookResult& outResult, std::string& outError)
        -> bool {
        outResult = {};
        outError.clear();

        FEnvMapCookOptions options{};
        std::string        optError;
        if (!LoadEnvMapCookOptions(sourcePath, options, optError)) {
            outError = optError;
            return false;
        }

        FHDRImage image{};
        if (!DecodeRadianceHdr(sourceBytes, image, outError)) {
            return false;
        }

        const u32 size = options.SkyboxSize;
        if (size == 0U) {
            outError = "EnvMapSkyboxSize must be > 0.";
            return false;
        }

        const u32                                      mipCount = FullMipCount(size);
        std::array<std::vector<std::vector<float>>, 6> faceMipChains;
        for (u32 f = 0U; f < 6U; ++f) {
            faceMipChains[f].resize(mipCount);

            if (!GenerateSkyboxFace(image, static_cast<ECubeFace>(f), size, faceMipChains[f][0])) {
                outError = "Failed to generate skybox face.";
                return false;
            }

            u32 w = size;
            u32 h = size;
            for (u32 mip = 1U; mip < mipCount; ++mip) {
                u32 nw = 0U, nh = 0U;
                if (!DownsampleBox2x(
                        faceMipChains[f][mip - 1U], w, h, faceMipChains[f][mip], nw, nh)) {
                    outError = "Failed to downsample skybox mip.";
                    return false;
                }
                w = nw;
                h = nh;
            }
        }

        std::vector<u8>     cooked;
        Asset::FCubeMapDesc desc{};
        std::string         cookError;
        if (!BuildCubeMapRgba16fBlobFromMipChains(size, faceMipChains, cooked, desc, cookError)) {
            outError = cookError;
            return false;
        }

        // Stable cook key extras: options relevant to cube generation.
        outResult.CookKeyExtras.resize(sizeof(u32));
        std::memcpy(outResult.CookKeyExtras.data(), &options.SkyboxSize, sizeof(u32));

        outResult.CookedBytes = Move(cooked);
        outResult.Desc        = desc;
        return true;
    }
} // namespace AltinaEngine::Tools::AssetPipeline
