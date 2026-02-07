#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "ShaderCompiler/ShaderCompiler.h"
#include "TestHarness.h"

namespace {
    using AltinaEngine::ShaderCompiler::EShaderSourceLanguage;
    using AltinaEngine::ShaderCompiler::EShaderStage;
    using AltinaEngine::ShaderCompiler::FShaderCompileRequest;
    using AltinaEngine::ShaderCompiler::FShaderCompileResult;
    using AltinaEngine::ShaderCompiler::GetShaderCompiler;
    using AltinaEngine::Core::Container::FString;
    using AltinaEngine::Rhi::ERhiBackend;

    auto ToFString(const std::filesystem::path& path) -> FString {
        FString out;
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        const auto wide = path.wstring();
        if (!wide.empty()) {
            out.Append(wide.c_str(), wide.size());
        }
#else
        const auto narrow = path.string();
        if (!narrow.empty()) {
            out.Append(narrow.c_str(), narrow.size());
        }
#endif
        return out;
    }

    auto ToAsciiString(const FString& text) -> std::string {
        std::string out;
        const auto* data = text.GetData();
        const auto  size = text.Length();
        out.reserve(size);
        for (AltinaEngine::usize i = 0; i < size; ++i) {
            const auto ch = data[i];
            out.push_back((ch <= 0x7f) ? static_cast<char>(ch) : '?');
        }
        return out;
    }

    auto IsCompilerUnavailable(const FShaderCompileResult& result) -> bool {
        const auto diag = ToAsciiString(result.mDiagnostics);
        if (diag.find("disabled") != std::string::npos) {
            return true;
        }
        if (diag.find("Failed to launch compiler process.") != std::string::npos) {
            return true;
        }
        if (diag.find("Process execution not supported") != std::string::npos) {
            return true;
        }
        return false;
    }

    auto WriteTempShaderFile(const char* prefix, const char* content) -> std::filesystem::path {
        static std::atomic<unsigned int> counter{ 0 };
        std::error_code                  ec;
        auto                             dir = std::filesystem::temp_directory_path(ec);
        if (ec) {
            dir = std::filesystem::current_path();
        }

        dir /= "AltinaEngine";
        dir /= "ShaderCompileTests";
        std::filesystem::create_directories(dir, ec);

        const auto id = counter.fetch_add(1, std::memory_order_relaxed);
        std::filesystem::path path = dir / (std::string(prefix) + "_" + std::to_string(id) + ".hlsl");

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(content, static_cast<std::streamsize>(std::strlen(content)));
        file.close();
        return path;
    }

    auto CompileShader(const char* source, const AltinaEngine::TChar* entryPoint, EShaderStage stage,
        EShaderSourceLanguage language, ERhiBackend backend, const char* label) -> bool {
        const auto shaderPath = WriteTempShaderFile(label, source);

        FShaderCompileRequest request;
        request.mSource.mPath       = ToFString(shaderPath);
        request.mSource.mEntryPoint = FString(entryPoint);
        request.mSource.mStage      = stage;
        request.mSource.mLanguage   = language;
        request.mOptions.mTargetBackend = backend;

        const auto result = GetShaderCompiler().Compile(request);

        std::error_code ec;
        std::filesystem::remove(shaderPath, ec);

        if (!result.mSucceeded && IsCompilerUnavailable(result)) {
            std::cout << "[ SKIP ] " << label << " compiler unavailable\n";
            return false;
        }

        if (!result.mSucceeded) {
            std::cerr << "[FAIL] " << label << " compile diagnostics:\n"
                      << ToAsciiString(result.mDiagnostics) << "\n";
        }

        REQUIRE(result.mSucceeded);
        REQUIRE(!result.mBytecode.IsEmpty());
        return result.mSucceeded;
    }

    constexpr const char* kVsShader = R"(struct VSIn {
    float3 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOut VSMain(VSIn input) {
    VSOut o;
    o.pos = float4(input.pos, 1.0);
    o.uv = input.uv;
    return o;
}
)";

    constexpr const char* kPsShader = R"(Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);

float4 PSMain(float2 uv : TEXCOORD0) : SV_Target {
    return gTex.Sample(gSamp, uv);
}
)";

    constexpr const char* kCsShader = R"(RWTexture2D<float4> gOutTex : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    gOutTex[id.xy] = float4(1, 0, 0, 1);
}
)";
} // namespace

TEST_CASE("ShaderCompiler.DXC.VS_PS_CS") {
    const bool vsOk = CompileShader(kVsShader, TEXT("VSMain"), EShaderStage::Vertex,
        EShaderSourceLanguage::Hlsl, ERhiBackend::DirectX12, "DXC-VS");
    if (!vsOk) {
        return;
    }
    REQUIRE(CompileShader(kPsShader, TEXT("PSMain"), EShaderStage::Pixel,
        EShaderSourceLanguage::Hlsl, ERhiBackend::DirectX12, "DXC-PS"));
    REQUIRE(CompileShader(kCsShader, TEXT("CSMain"), EShaderStage::Compute,
        EShaderSourceLanguage::Hlsl, ERhiBackend::DirectX12, "DXC-CS"));
}

TEST_CASE("ShaderCompiler.Slang.VS_PS_CS") {
    const bool vsOk = CompileShader(kVsShader, TEXT("VSMain"), EShaderStage::Vertex,
        EShaderSourceLanguage::Slang, ERhiBackend::Vulkan, "Slang-VS");
    if (!vsOk) {
        return;
    }
    REQUIRE(CompileShader(kPsShader, TEXT("PSMain"), EShaderStage::Pixel,
        EShaderSourceLanguage::Slang, ERhiBackend::Vulkan, "Slang-PS"));
    REQUIRE(CompileShader(kCsShader, TEXT("CSMain"), EShaderStage::Compute,
        EShaderSourceLanguage::Slang, ERhiBackend::Vulkan, "Slang-CS"));
}
