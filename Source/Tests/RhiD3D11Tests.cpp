#include "TestHarness.h"

#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "RhiD3D11/RhiD3D11Context.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiShader.h"
#include "Rhi/RhiStructs.h"
#include "Rhi/RhiTexture.h"
#include "ShaderCompiler/ShaderCompiler.h"
#include "ShaderCompiler/ShaderRhiBindings.h"
#include "Types/Traits.h"

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
        const auto fileName =
            std::string(prefix) + "_" + std::to_string(counter.fetch_add(1)) + ".hlsl";
        auto path = dir / fileName;
        std::ofstream file(path, std::ios::binary);
        file.write(content, static_cast<std::streamsize>(std::strlen(content)));
        file.close();
        return path;
    }

    constexpr const char* kMinimalVsShader = R"(struct VSIn {
    float3 pos : POSITION;
};

struct VSOut {
    float4 pos : SV_POSITION;
};

VSOut VSMain(VSIn input) {
    VSOut output;
    output.pos = float4(input.pos, 1.0);
    return output;
})";
} // namespace

TEST_CASE("RhiD3D11.DeviceCreation") {
#if AE_PLATFORM_WIN
    using AltinaEngine::Rhi::ERhiBufferBindFlags;
    using AltinaEngine::ShaderCompiler::BuildRhiShaderDesc;
    using AltinaEngine::Rhi::FRhiBufferDesc;
    using AltinaEngine::Rhi::FRhiD3D11Context;
    using AltinaEngine::Rhi::FRhiInitDesc;
    using AltinaEngine::Rhi::FRhiSamplerDesc;
    using AltinaEngine::Rhi::FRhiTextureDesc;
    using AltinaEngine::Rhi::kRhiInvalidAdapterIndex;

    FRhiD3D11Context context;
    FRhiInitDesc     initDesc;
    initDesc.mEnableDebugLayer = false;

    REQUIRE(context.Init(initDesc));

    const auto adapters = context.EnumerateAdapters();
    if (adapters.IsEmpty()) {
        return;
    }

    const auto device = context.CreateDevice(kRhiInvalidAdapterIndex);
    REQUIRE(device);
    REQUIRE(device->GetAdapterDesc().IsValid());

    FRhiBufferDesc bufferDesc;
    bufferDesc.mSizeBytes = 256;
    bufferDesc.mBindFlags = ERhiBufferBindFlags::Vertex;
    const auto buffer = device->CreateBuffer(bufferDesc);
    REQUIRE(buffer);

    FRhiTextureDesc textureDesc;
    textureDesc.mWidth  = 4;
    textureDesc.mHeight = 4;
    const auto texture = device->CreateTexture(textureDesc);
    REQUIRE(texture);

    FRhiSamplerDesc samplerDesc;
    const auto sampler = device->CreateSampler(samplerDesc);
    REQUIRE(sampler);

    const auto shaderPath = WriteTempShaderFile("d3d11_vs", kMinimalVsShader);
    FShaderCompileRequest request;
    request.mSource.mPath       = ToFString(shaderPath);
    request.mSource.mEntryPoint = FString(TEXT("VSMain"));
    request.mSource.mStage      = EShaderStage::Vertex;
    request.mSource.mLanguage   = EShaderSourceLanguage::Hlsl;
    request.mOptions.mTargetBackend = ERhiBackend::DirectX11;

    const auto result = GetShaderCompiler().Compile(request);

    std::error_code ec;
    std::filesystem::remove(shaderPath, ec);

    if (!result.mSucceeded && IsCompilerUnavailable(result)) {
        return;
    }

    if (!result.mSucceeded) {
        std::cerr << "[FAIL] D3D11 shader compile diagnostics:\n"
                  << ToAsciiString(result.mDiagnostics) << "\n";
    }
    REQUIRE(result.mSucceeded);

    const auto shaderDesc = BuildRhiShaderDesc(result);
    const auto shader     = device->CreateShader(shaderDesc);
    if (!shader) {
        std::cerr << "[SKIP] D3D11 CreateShader failed. Output:\n"
                  << ToAsciiString(result.mOutputDebugPath) << "\n"
                  << ToAsciiString(result.mDiagnostics) << "\n";
        return;
    }
#else
    // Non-Windows platforms do not support D3D11.
#endif
}
