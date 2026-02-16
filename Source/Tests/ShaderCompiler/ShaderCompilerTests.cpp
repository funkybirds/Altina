#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "ShaderCompiler/ShaderCompiler.h"
#include "TestHarness.h"
#include "Shader/ShaderPropertyBag.h"
#include "RhiMock/RhiMockContext.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiPipelineLayout.h"

using AltinaEngine::Core::Container::TShared;
using AltinaEngine::Core::Container::TVector;
namespace {
    namespace Container = AltinaEngine::Core::Container;
    using AltinaEngine::Rhi::ERhiAdapterType;
    using AltinaEngine::Rhi::ERhiBackend;
    using AltinaEngine::Rhi::ERhiVendorId;
    using AltinaEngine::Rhi::FRhiAdapterDesc;
    using AltinaEngine::Rhi::FRhiInitDesc;
    using AltinaEngine::Rhi::FRhiMockContext;
    using AltinaEngine::ShaderCompiler::EShaderSourceLanguage;
    using AltinaEngine::ShaderCompiler::EShaderStage;
    using AltinaEngine::ShaderCompiler::FShaderCompileRequest;
    using AltinaEngine::ShaderCompiler::FShaderCompileResult;
    using AltinaEngine::ShaderCompiler::GetShaderCompiler;
    using Container::FString;

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

    auto GetShaderIncludeDir() -> std::filesystem::path {
#if defined(AE_SOURCE_DIR)
        return std::filesystem::path(AE_SOURCE_DIR) / "Source";
#else
        return std::filesystem::current_path();
#endif
    }

    void AddShaderIncludeDir(FShaderCompileRequest& request) {
        request.mSource.mIncludeDirs.PushBack(ToFString(GetShaderIncludeDir()));
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

    auto CreateMockDevice(FRhiMockContext& context) -> TShared<AltinaEngine::Rhi::FRhiDevice> {
        FRhiAdapterDesc adapter{};
        adapter.mName.Assign(TEXT("Mock GPU"));
        adapter.mType     = ERhiAdapterType::Discrete;
        adapter.mVendorId = ERhiVendorId::Nvidia;
        context.AddAdapter(adapter);
        REQUIRE(context.Init(FRhiInitDesc{}));
        auto device = context.CreateDevice(0);
        REQUIRE(device);
        return device;
    }

    auto BuildPipelineLayoutFromResult(AltinaEngine::Rhi::FRhiDevice& device,
        const FShaderCompileResult&                                   result,
        TVector<AltinaEngine::Rhi::FRhiBindGroupLayoutRef>&           outLayouts)
        -> AltinaEngine::Rhi::FRhiPipelineLayoutRef {
        auto pipelineDesc = result.mRhiLayout.mPipelineLayout;
        pipelineDesc.mBindGroupLayouts.Clear();
        pipelineDesc.mBindGroupLayouts.Reserve(result.mRhiLayout.mBindGroupLayouts.Size());
        outLayouts.Clear();
        outLayouts.Reserve(result.mRhiLayout.mBindGroupLayouts.Size());
        for (const auto& layoutDesc : result.mRhiLayout.mBindGroupLayouts) {
            auto layoutRef = device.CreateBindGroupLayout(layoutDesc);
            REQUIRE(layoutRef);
            outLayouts.PushBack(layoutRef);
            pipelineDesc.mBindGroupLayouts.PushBack(layoutRef.Get());
        }
        auto pipelineLayout = device.CreatePipelineLayout(pipelineDesc);
        REQUIRE(pipelineLayout);
        return pipelineLayout;
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

        const auto            id = counter.fetch_add(1, std::memory_order_relaxed);
        std::filesystem::path path =
            dir / (std::string(prefix) + "_" + std::to_string(id) + ".hlsl");

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(content, static_cast<std::streamsize>(std::strlen(content)));
        file.close();
        return path;
    }

    auto CompileShader(const char* source, const AltinaEngine::TChar* entryPoint,
        EShaderStage stage, EShaderSourceLanguage language, ERhiBackend backend, const char* label)
        -> bool {
        const auto            shaderPath = WriteTempShaderFile(label, source);

        FShaderCompileRequest request;
        request.mSource.mPath           = ToFString(shaderPath);
        request.mSource.mEntryPoint     = FString(entryPoint);
        request.mSource.mStage          = stage;
        request.mSource.mLanguage       = language;
        request.mOptions.mTargetBackend = backend;

        const auto      result = GetShaderCompiler().Compile(request);

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

    constexpr const char* kAutoBindingGroupedShaderHlsl =
        R"(#include "Shader/Bindings/ShaderBindings.hlsli"

AE_PER_FRAME_CBUFFER(PerFrame) {
    float4 mTint;
};

AE_PER_DRAW_CBUFFER(PerDraw) {
    float4x4 mWorld;
};

AE_PER_MATERIAL_SRV(Texture2D, gTex);
AE_PER_MATERIAL_SAMPLER(gSamp);
AE_PER_DRAW_UAV(RWTexture2D<float4>, gOut);

[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    float4 tex = gTex.SampleLevel(gSamp, float2(0.0, 0.0), 0);
    gOut[id.xy] = tex + mTint + mWorld._11;
}
)";

    constexpr const char* kAutoBindingGroupedShaderSlang =
        R"(#include "Shader/Bindings/ShaderBindings.slang"

AE_PER_FRAME_CBUFFER(PerFrame) {
    float4 mTint;
};

AE_PER_DRAW_CBUFFER(PerDraw) {
    float4x4 mWorld;
};

AE_PER_MATERIAL_SRV(Texture2D, gTex);
AE_PER_MATERIAL_SAMPLER(gSamp);
AE_PER_DRAW_UAV(RWTexture2D<float4>, gOut);

[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    float4 tex = gTex.SampleLevel(gSamp, float2(0.0, 0.0), 0);
    gOut[id.xy] = tex + mTint + mWorld._11;
}
)";

    constexpr const char* kCBufferMemberShaderHlsl =
        R"(#include "Shader/Bindings/ShaderBindings.hlsli"

struct FInner {
    float3 A;
    float  B;
    float4 C;
};

AE_PER_MATERIAL_CBUFFER(PerMaterial) {
    float4 BaseColor;
    FInner Inner;
    float2 UVScale;
    float2 UVBias;
};

RWStructuredBuffer<uint> gOut : register(u0);

[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    gOut[0] = asuint(BaseColor.x + Inner.B + UVScale.x + UVBias.y + (float)id.x);
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

TEST_CASE("ShaderCompiler.Slang.VulkanAutoBinding") {
    const auto shaderPath =
        WriteTempShaderFile("Slang-AutoBinding", kAutoBindingGroupedShaderSlang);

    FShaderCompileRequest request;
    request.mSource.mPath           = ToFString(shaderPath);
    request.mSource.mEntryPoint     = FString(TEXT("CSMain"));
    request.mSource.mStage          = EShaderStage::Compute;
    request.mSource.mLanguage       = EShaderSourceLanguage::Slang;
    request.mOptions.mTargetBackend = ERhiBackend::Vulkan;
    AddShaderIncludeDir(request);
    request.mOptions.mVulkanBinding.mEnableAutoShift     = true;
    request.mOptions.mVulkanBinding.mConstantBufferShift = 0U;
    request.mOptions.mVulkanBinding.mTextureShift        = 100U;
    request.mOptions.mVulkanBinding.mSamplerShift        = 200U;
    request.mOptions.mVulkanBinding.mStorageShift        = 300U;

    const auto      result = GetShaderCompiler().Compile(request);

    std::error_code ec;
    std::filesystem::remove(shaderPath, ec);

    if (!result.mSucceeded && IsCompilerUnavailable(result)) {
        std::cout << "[ SKIP ] Slang-AutoBinding compiler unavailable\n";
        return;
    }

    if (!result.mSucceeded) {
        std::cerr << "[FAIL] Slang-AutoBinding compile diagnostics:\n"
                  << ToAsciiString(result.mDiagnostics) << "\n";
    }

    REQUIRE(result.mSucceeded);
    REQUIRE(!result.mBytecode.IsEmpty());

    FRhiMockContext                                    context;
    auto                                               device = CreateMockDevice(context);
    TVector<AltinaEngine::Rhi::FRhiBindGroupLayoutRef> layouts;
    BuildPipelineLayoutFromResult(*device, result, layouts);

    auto FindResource =
        [&](const char* name) -> const AltinaEngine::ShaderCompiler::FShaderResourceBinding* {
        for (const auto& resource : result.mReflection.mResources) {
            if (ToAsciiString(resource.mName) == name) {
                return &resource;
            }
        }
        return nullptr;
    };

    const auto* cb     = FindResource("PerFrame");
    const auto* drawCb = FindResource("PerDraw");
    const auto* tex    = FindResource("gTex");
    const auto* samp   = FindResource("gSamp");
    const auto* out    = FindResource("gOut");

    REQUIRE(cb != nullptr);
    REQUIRE(drawCb != nullptr);
    REQUIRE(tex != nullptr);
    REQUIRE(samp != nullptr);
    REQUIRE(out != nullptr);
    if ((cb == nullptr) || (drawCb == nullptr) || (tex == nullptr) || (samp == nullptr)
        || (out == nullptr)) {
        return;
    }

    REQUIRE(cb->mSet == 0U);
    REQUIRE(drawCb->mSet == 1U);
    REQUIRE(tex->mSet == 2U);
    REQUIRE(samp->mSet == 2U);
    REQUIRE(out->mSet == 1U);

    REQUIRE(cb->mBinding == 0U);
    REQUIRE(drawCb->mBinding == 0U);
    REQUIRE(tex->mBinding == 100U);
    REQUIRE(samp->mBinding == 200U);
    REQUIRE(out->mBinding == 300U);
}

TEST_CASE("ShaderCompiler.DXC.AutoBindingDX12") {
    const auto shaderPath = WriteTempShaderFile("DXC-AutoBinding", kAutoBindingGroupedShaderHlsl);

    FShaderCompileRequest request;
    request.mSource.mPath           = ToFString(shaderPath);
    request.mSource.mEntryPoint     = FString(TEXT("CSMain"));
    request.mSource.mStage          = EShaderStage::Compute;
    request.mSource.mLanguage       = EShaderSourceLanguage::Hlsl;
    request.mOptions.mTargetBackend = ERhiBackend::DirectX12;
    AddShaderIncludeDir(request);

    const auto      result = GetShaderCompiler().Compile(request);

    std::error_code ec;
    std::filesystem::remove(shaderPath, ec);

    if (!result.mSucceeded && IsCompilerUnavailable(result)) {
        std::cout << "[ SKIP ] DXC-AutoBinding compiler unavailable\n";
        return;
    }

    if (!result.mSucceeded) {
        std::cerr << "[FAIL] DXC-AutoBinding compile diagnostics:\n"
                  << ToAsciiString(result.mDiagnostics) << "\n";
    }

    REQUIRE(result.mSucceeded);
    REQUIRE(!result.mBytecode.IsEmpty());

    FRhiMockContext                                    context;
    auto                                               device = CreateMockDevice(context);
    TVector<AltinaEngine::Rhi::FRhiBindGroupLayoutRef> layouts;
    BuildPipelineLayoutFromResult(*device, result, layouts);

    auto FindResource =
        [&](const char* name) -> const AltinaEngine::ShaderCompiler::FShaderResourceBinding* {
        for (const auto& resource : result.mReflection.mResources) {
            if (ToAsciiString(resource.mName) == name) {
                return &resource;
            }
        }
        return nullptr;
    };

    const auto* cb     = FindResource("PerFrame");
    const auto* drawCb = FindResource("PerDraw");
    const auto* tex    = FindResource("gTex");
    const auto* samp   = FindResource("gSamp");
    const auto* out    = FindResource("gOut");

    REQUIRE(cb != nullptr);
    REQUIRE(drawCb != nullptr);
    REQUIRE(tex != nullptr);
    REQUIRE(samp != nullptr);
    REQUIRE(out != nullptr);
    if ((cb == nullptr) || (drawCb == nullptr) || (tex == nullptr) || (samp == nullptr)
        || (out == nullptr)) {
        return;
    }

    REQUIRE(cb->mSet == 0U);
    REQUIRE(drawCb->mSet == 1U);
    REQUIRE(tex->mSet == 2U);
    REQUIRE(samp->mSet == 2U);
    REQUIRE(out->mSet == 1U);

    REQUIRE(cb->mBinding == 0U);
    REQUIRE(drawCb->mBinding == 0U);
    REQUIRE(tex->mBinding == 0U);
    REQUIRE(samp->mBinding == 0U);
    REQUIRE(out->mBinding == 0U);
}

TEST_CASE("ShaderCompiler.DXC.AutoBindingDX11") {
    const auto shaderPath =
        WriteTempShaderFile("DXC-AutoBinding-DX11", kAutoBindingGroupedShaderHlsl);

    FShaderCompileRequest request;
    request.mSource.mPath           = ToFString(shaderPath);
    request.mSource.mEntryPoint     = FString(TEXT("CSMain"));
    request.mSource.mStage          = EShaderStage::Compute;
    request.mSource.mLanguage       = EShaderSourceLanguage::Hlsl;
    request.mOptions.mTargetBackend = ERhiBackend::DirectX11;
    AddShaderIncludeDir(request);

    const auto      result = GetShaderCompiler().Compile(request);

    std::error_code ec;
    std::filesystem::remove(shaderPath, ec);

    if (!result.mSucceeded && IsCompilerUnavailable(result)) {
        std::cout << "[ SKIP ] DXC-AutoBinding-DX11 compiler unavailable\n";
        return;
    }

    if (!result.mSucceeded) {
        std::cerr << "[FAIL] DXC-AutoBinding-DX11 compile diagnostics:\n"
                  << ToAsciiString(result.mDiagnostics) << "\n";
    }

    REQUIRE(result.mSucceeded);
    REQUIRE(!result.mBytecode.IsEmpty());

    FRhiMockContext                                    context;
    auto                                               device = CreateMockDevice(context);
    TVector<AltinaEngine::Rhi::FRhiBindGroupLayoutRef> layouts;
    BuildPipelineLayoutFromResult(*device, result, layouts);

    auto FindResource =
        [&](const char* name) -> const AltinaEngine::ShaderCompiler::FShaderResourceBinding* {
        for (const auto& resource : result.mReflection.mResources) {
            if (ToAsciiString(resource.mName) == name) {
                return &resource;
            }
        }
        return nullptr;
    };

    const auto* cb     = FindResource("PerFrame");
    const auto* drawCb = FindResource("PerDraw");
    const auto* tex    = FindResource("gTex");
    const auto* samp   = FindResource("gSamp");
    const auto* out    = FindResource("gOut");

    REQUIRE(cb != nullptr);
    REQUIRE(drawCb != nullptr);
    REQUIRE(tex != nullptr);
    REQUIRE(samp != nullptr);
    REQUIRE(out != nullptr);
    if ((cb == nullptr) || (drawCb == nullptr) || (tex == nullptr) || (samp == nullptr)
        || (out == nullptr)) {
        return;
    }

    REQUIRE(cb->mSet == 0U);
    REQUIRE(drawCb->mSet == 0U);
    REQUIRE(tex->mSet == 0U);
    REQUIRE(samp->mSet == 0U);
    REQUIRE(out->mSet == 0U);

    REQUIRE(cb->mBinding == 0U);
    REQUIRE(drawCb->mBinding == 4U);
    REQUIRE(tex->mBinding == 32U);
    REQUIRE(samp->mBinding == 8U);
    REQUIRE(out->mBinding == 4U);
}

TEST_CASE("ShaderCompiler.DXC.ConstantBufferMembers") {
    const auto shaderPath = WriteTempShaderFile("DXC-CBufferMembers", kCBufferMemberShaderHlsl);

    FShaderCompileRequest request;
    request.mSource.mPath           = ToFString(shaderPath);
    request.mSource.mEntryPoint     = FString(TEXT("CSMain"));
    request.mSource.mStage          = EShaderStage::Compute;
    request.mSource.mLanguage       = EShaderSourceLanguage::Hlsl;
    request.mOptions.mTargetBackend = ERhiBackend::DirectX12;
    AddShaderIncludeDir(request);

    const auto      result = GetShaderCompiler().Compile(request);

    std::error_code ec;
    std::filesystem::remove(shaderPath, ec);

    if (!result.mSucceeded && IsCompilerUnavailable(result)) {
        std::cout << "[ SKIP ] DXC-CBufferMembers compiler unavailable\n";
        return;
    }

    if (!result.mSucceeded) {
        std::cerr << "[FAIL] DXC-CBufferMembers compile diagnostics:\n"
                  << ToAsciiString(result.mDiagnostics) << "\n";
    }

    REQUIRE(result.mSucceeded);
    REQUIRE(!result.mReflection.mConstantBuffers.IsEmpty());

    auto FindCBuffer = [&](const char* name) -> const AltinaEngine::Shader::FShaderConstantBuffer* {
        for (const auto& cb : result.mReflection.mConstantBuffers) {
            if (ToAsciiString(cb.mName) == name) {
                return &cb;
            }
        }
        return nullptr;
    };

    auto FindMember =
        [&](const AltinaEngine::Shader::FShaderConstantBuffer& cb,
            const char* name) -> const AltinaEngine::Shader::FShaderConstantBufferMember* {
        for (const auto& member : cb.mMembers) {
            if (ToAsciiString(member.mName) == name) {
                return &member;
            }
        }
        return nullptr;
    };

    const auto* cb = FindCBuffer("PerMaterial");
    REQUIRE(cb != nullptr);
    if (cb == nullptr) {
        return;
    }

    REQUIRE(cb->mSet == 2U);
    REQUIRE(cb->mBinding == 0U);
    REQUIRE(cb->mSizeBytes >= 64U);

    const auto* baseColorMember = FindMember(*cb, "BaseColor");
    const auto* innerMember     = FindMember(*cb, "Inner");
    const auto* innerAMember    = FindMember(*cb, "Inner.A");
    const auto* innerBMember    = FindMember(*cb, "Inner.B");
    const auto* innerCMember    = FindMember(*cb, "Inner.C");
    const auto* uvScaleMember   = FindMember(*cb, "UVScale");
    const auto* uvBiasMember    = FindMember(*cb, "UVBias");

    REQUIRE(baseColorMember != nullptr);
    REQUIRE(innerMember != nullptr);
    REQUIRE(innerAMember != nullptr);
    REQUIRE(innerBMember != nullptr);
    REQUIRE(innerCMember != nullptr);
    REQUIRE(uvScaleMember != nullptr);
    REQUIRE(uvBiasMember != nullptr);
    if (!baseColorMember || !innerMember || !innerAMember || !innerBMember || !innerCMember
        || !uvScaleMember || !uvBiasMember) {
        return;
    }

    REQUIRE(baseColorMember->mOffset == 0U);
    REQUIRE(baseColorMember->mSize == 16U);

    REQUIRE(innerMember->mOffset == 16U);
    REQUIRE(innerMember->mSize == 32U);

    REQUIRE(innerAMember->mOffset == 16U);
    REQUIRE(innerAMember->mSize == 12U);

    REQUIRE(innerBMember->mOffset == 28U);
    REQUIRE(innerBMember->mSize == 4U);

    REQUIRE(innerCMember->mOffset == 32U);
    REQUIRE(innerCMember->mSize == 16U);

    REQUIRE(uvScaleMember->mOffset == 48U);
    REQUIRE(uvScaleMember->mSize == 8U);

    REQUIRE(uvBiasMember->mOffset == 56U);
    REQUIRE(uvBiasMember->mSize == 8U);

    AltinaEngine::Shader::FShaderPropertyBag bag(*cb);
    const float                              baseColorValue[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    const float                              innerBValue       = 5.0f;
    const float                              uvBiasValue[2]    = { 6.0f, 7.0f };

    REQUIRE(bag.SetRaw(TEXT("BaseColor"), baseColorValue, sizeof(baseColorValue)));
    REQUIRE(bag.Set(TEXT("Inner.B"), innerBValue));
    REQUIRE(bag.SetRaw(TEXT("UVBias"), uvBiasValue, sizeof(uvBiasValue)));

    float baseColorRead[4] = {};
    float innerBRead       = 0.0f;
    float uvBiasRead[2]    = {};

    std::memcpy(baseColorRead, bag.GetData() + baseColorMember->mOffset, sizeof(baseColorRead));
    std::memcpy(&innerBRead, bag.GetData() + innerBMember->mOffset, sizeof(innerBRead));
    std::memcpy(uvBiasRead, bag.GetData() + uvBiasMember->mOffset, sizeof(uvBiasRead));

    REQUIRE_EQ(baseColorRead[0], baseColorValue[0]);
    REQUIRE_EQ(baseColorRead[1], baseColorValue[1]);
    REQUIRE_EQ(baseColorRead[2], baseColorValue[2]);
    REQUIRE_EQ(baseColorRead[3], baseColorValue[3]);
    REQUIRE_EQ(innerBRead, innerBValue);
    REQUIRE_EQ(uvBiasRead[0], uvBiasValue[0]);
    REQUIRE_EQ(uvBiasRead[1], uvBiasValue[1]);
}
