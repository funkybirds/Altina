#include "Base/AltinaBase.h"
#include "Gameplay/GameplayModule.h"
#include "Launch/EngineLoop.h"
#include "Input/InputSystem.h"
#include "Input/Keys.h"
#include "Reflection/Reflection.h"
#include "Rhi/Command/RhiCmdBuiltins.h"
#include "Rhi/Command/RhiCmdContextAdapter.h"
#include "Rhi/Command/RhiCmdExecutor.h"
#include "Rhi/Command/RhiCmdList.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiResourceView.h"
#include "Rhi/RhiShader.h"
#include "Rhi/RhiStructs.h"
#include "Rhi/RhiViewport.h"
#include "RhiD3D11/RhiD3D11CommandContext.h"
#include "ShaderCompiler/ShaderCompiler.h"
#include "Types/Aliases.h"
#include "Container/String.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

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
    #include <Windows.h>
    #include <d3dcompiler.h>
    #include <wrl/client.h>
#endif

using namespace AltinaEngine;
using namespace AltinaEngine::Core;
using namespace AltinaEngine::Launch;

struct Neko {
    int    mNya  = 114;
    double mMeow = 1.0;
};

namespace {
#if AE_PLATFORM_WIN
    namespace ShaderCompileHelpers {
        using Microsoft::WRL::ComPtr;

        auto ToFString(const std::filesystem::path& path) -> Container::FString {
            Container::FString out;
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

        auto ToFStringAscii(const char* text) -> Container::FString {
            Container::FString out;
            if (text == nullptr) {
                return out;
            }
            const size_t length = std::strlen(text);
            out.Reserve(length);
            for (size_t i = 0; i < length; ++i) {
                out.Append(static_cast<TChar>(text[i]));
            }
            return out;
        }

        auto ToAsciiString(const Container::FString& text) -> Container::FNativeString {
            Container::FNativeString out;
            const auto* data = text.GetData();
            const auto  size = text.Length();
            out.Reserve(size);
            for (usize i = 0; i < size; ++i) {
                const auto ch = data[i];
                out.Append((static_cast<u32>(ch) <= 0x7fU) ? static_cast<char>(ch) : '?');
            }
            return out;
        }

        auto IsCompilerUnavailable(const Container::FString& diagnostics) -> bool {
            const auto diag = ToAsciiString(diagnostics);
            const auto view = diag.ToView();
            if (view.Contains(Container::FNativeStringView("disabled"))) {
                return true;
            }
            if (view.Contains(Container::FNativeStringView("Failed to launch compiler process."))) {
                return true;
            }
            if (view.Contains(Container::FNativeStringView("Process execution not supported"))) {
                return true;
            }
            return false;
        }

        auto GetEnvPath(const char* name, std::filesystem::path& outPath) -> bool {
            if (name == nullptr) {
                return false;
            }
            const char* value = std::getenv(name);
            if (value == nullptr || value[0] == '\0') {
                return false;
            }
            outPath = std::filesystem::path(value);
            return true;
        }

        auto TryResolveDxcPath(std::filesystem::path& outPath) -> bool {
            std::filesystem::path envPath;
            if (GetEnvPath("AE_DXC_PATH", envPath) || GetEnvPath("DXC_PATH", envPath)) {
                if (std::filesystem::exists(envPath)) {
                    outPath = envPath;
                    return true;
                }
            }

            if (GetEnvPath("VULKAN_SDK", envPath)) {
                auto candidate = envPath / "Bin" / "dxc.exe";
                if (std::filesystem::exists(candidate)) {
                    outPath = candidate;
                    return true;
                }
            }

            if (GetEnvPath("ProgramFiles(x86)", envPath) || GetEnvPath("ProgramFiles", envPath)) {
                auto            kitsRoot = envPath / "Windows Kits" / "10" / "bin";
                std::error_code ec;
                if (std::filesystem::exists(kitsRoot, ec)) {
                    std::filesystem::path bestPath;
                    for (const auto& entry : std::filesystem::directory_iterator(kitsRoot, ec)) {
                        if (ec || !entry.is_directory()) {
                            continue;
                        }
                        auto candidate = entry.path() / "x64" / "dxc.exe";
                        if (std::filesystem::exists(candidate)) {
                            if (bestPath.empty() || candidate.string() > bestPath.string()) {
                                bestPath = candidate;
                            }
                        }
                    }
                    if (!bestPath.empty()) {
                        outPath = bestPath;
                        return true;
                    }
                }
            }

            return false;
        }

        auto TryResolveSlangcPath(std::filesystem::path& outPath) -> bool {
            std::filesystem::path envPath;
            if (GetEnvPath("AE_SLANGC_PATH", envPath) || GetEnvPath("SLANGC_PATH", envPath)) {
                if (std::filesystem::exists(envPath)) {
                    outPath = envPath;
                    return true;
                }
            }

            if (GetEnvPath("VULKAN_SDK", envPath)) {
                auto candidate = envPath / "Bin" / "slangc.exe";
                if (std::filesystem::exists(candidate)) {
                    outPath = candidate;
                    return true;
                }
            }

            return false;
        }

        auto CompileD3D11ShaderFxc(const char* source, const char* entryPoint,
            const char* targetProfile, Shader::FShaderBytecode& outBytecode,
            Container::FNativeString& outErrors)
            -> bool {
            if (!source || !entryPoint || !targetProfile) {
                return false;
            }

            ComPtr<ID3DBlob> bytecode;
            ComPtr<ID3DBlob> errors;
            const UINT       flags = D3DCOMPILE_ENABLE_STRICTNESS;
            const HRESULT    hr = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr,
                   entryPoint, targetProfile, flags, 0, &bytecode, &errors);

            outErrors.Clear();
            if (errors) {
                const auto* data = static_cast<const char*>(errors->GetBufferPointer());
                outErrors.Append(data, static_cast<usize>(errors->GetBufferSize()));
            }

            if (FAILED(hr) || !bytecode) {
                return false;
            }

            const SIZE_T size = bytecode->GetBufferSize();
            outBytecode.mData.Resize(static_cast<usize>(size));
            std::memcpy(outBytecode.mData.Data(), bytecode->GetBufferPointer(), size);
            return true;
        }

        auto WriteTempShaderFile(const char* prefix, const char* content,
            Container::FNativeString& outErrors)
            -> std::filesystem::path {
            if (content == nullptr) {
                outErrors = "Shader source is null.";
                return {};
            }

            static std::atomic<unsigned int> counter{ 0 };
            std::error_code                  ec;
            auto                             dir = std::filesystem::temp_directory_path(ec);
            if (ec) {
                dir = std::filesystem::current_path();
            }

            dir /= "AltinaEngine";
            dir /= "Demo";
            dir /= "Minimal";
            std::filesystem::create_directories(dir, ec);
            if (ec) {
                outErrors = "Failed to create shader temp directory.";
                return {};
            }

            const auto            id = counter.fetch_add(1, std::memory_order_relaxed);
            Container::FNativeString fileName(prefix);
            fileName.Append("_");
            fileName.AppendNumber(id);
            fileName.Append(".hlsl");
            std::filesystem::path path = dir / std::filesystem::path(fileName.CStr());

            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            if (!file) {
                outErrors = "Failed to open temp shader file for writing.";
                return {};
            }

            file.write(content, static_cast<std::streamsize>(std::strlen(content)));
            if (!file) {
                outErrors = "Failed to write temp shader file.";
                return {};
            }

            file.close();
            return path;
        }

        auto CompileD3D11ShaderWithShaderCompiler(const char* source, const char* entryPoint,
            Shader::EShaderStage stage, ShaderCompiler::EShaderSourceLanguage sourceLanguage,
            Shader::FShaderBytecode& outBytecode, Shader::FShaderReflection& outReflection,
            Container::FNativeString& outErrors, const char* targetProfile = nullptr) -> bool {
            if (!source || !entryPoint) {
                return false;
            }

            outErrors.Clear();
            const auto shaderPath = WriteTempShaderFile("TriangleShader", source, outErrors);
            if (shaderPath.empty()) {
                return false;
            }

            ShaderCompiler::FShaderCompileRequest request;
            request.mSource.mPath           = ToFString(shaderPath);
            request.mSource.mEntryPoint     = ToFStringAscii(entryPoint);
            request.mSource.mStage          = stage;
            request.mSource.mLanguage       = sourceLanguage;
            request.mOptions.mTargetBackend = Rhi::ERhiBackend::DirectX11;
            if (targetProfile != nullptr && targetProfile[0] != '\0') {
                request.mOptions.mTargetProfile = ToFStringAscii(targetProfile);
            }

            auto result = ShaderCompiler::GetShaderCompiler().Compile(request);
            if ((!result.mSucceeded || result.mBytecode.IsEmpty())
                && IsCompilerUnavailable(result.mDiagnostics)) {
                std::filesystem::path compilerPath;
                const bool            resolved =
                    (sourceLanguage == ShaderCompiler::EShaderSourceLanguage::Slang)
                               ? TryResolveSlangcPath(compilerPath)
                               : TryResolveDxcPath(compilerPath);
                if (resolved) {
                    request.mOptions.mCompilerPathOverride = ToFString(compilerPath);
                    result = ShaderCompiler::GetShaderCompiler().Compile(request);
                }
            }

            std::error_code ec;
            std::filesystem::remove(shaderPath, ec);

            outErrors = ToAsciiString(result.mDiagnostics);
            if (!result.mSucceeded || result.mBytecode.IsEmpty()) {
                return false;
            }

            outBytecode.mData = AltinaEngine::Move(result.mBytecode);
            outReflection     = AltinaEngine::Move(result.mReflection);
            return true;
        }

        auto CreateShaderFromBytecode(Rhi::FRhiDevice& device, Shader::EShaderStage stage,
            Shader::FShaderBytecode&& bytecode, Shader::FShaderReflection&& reflection)
            -> Rhi::FRhiShaderRef {
            Rhi::FRhiShaderDesc desc{};
            desc.mStage      = stage;
            desc.mBytecode   = AltinaEngine::Move(bytecode);
            desc.mReflection = AltinaEngine::Move(reflection);
            return device.CreateShader(desc);
        }
    } // namespace ShaderCompileHelpers
#endif

    constexpr const char* kTriangleShaderHlsl = R"(struct VSOut {
    float4 pos : SV_POSITION;
    float3 color : COLOR;
};

VSOut VSMain(uint vertexId : SV_VertexID) {
    VSOut output;
    float2 positions[3] = {
        float2(0.0f, 0.5f),
        float2(0.5f, -0.5f),
        float2(-0.5f, -0.5f)
    };
    float3 colors[3] = {
        float3(1.0f, 0.0f, 0.0f),
        float3(0.0f, 1.0f, 0.0f),
        float3(0.0f, 0.0f, 1.0f)
    };
    output.pos = float4(positions[vertexId], 0.0f, 1.0f);
    output.color = colors[vertexId];
    return output;
}

float4 PSMain(VSOut input) : SV_Target0 {
    return float4(input.color, 1.0f);
}
)";

    class FTriangleRenderer {
    public:
        void Render(Rhi::FRhiDevice& device, Rhi::FRhiViewport& viewport, u32 width, u32 height) {
#if AE_PLATFORM_WIN
            if (!mInitialized) {
                if (!Initialize(device)) {
                    return;
                }
            }

            if (width == 0U || height == 0U) {
                return;
            }

            auto* backBuffer = viewport.GetBackBuffer();
            if (backBuffer == nullptr || !mCommandContext || !mQueue) {
                return;
            }

            auto* d3dContext = static_cast<Rhi::FRhiD3D11CommandContext*>(mCommandContext.Get());
            Rhi::FRhiCmdContextAdapter adapter(*d3dContext);

            adapter.Begin();
            adapter.RHISetGraphicsPipeline(mPipeline.Get());

            mCmdList.Reset();

            Rhi::FRhiRenderTargetViewDesc rtvDesc{};
            rtvDesc.mTexture   = backBuffer;
            auto backBufferRtv = device.CreateRenderTargetView(rtvDesc);

            Rhi::FRhiRenderPassColorAttachment colorAttachment{};
            colorAttachment.mView       = backBufferRtv.Get();
            colorAttachment.mLoadOp     = Rhi::ERhiLoadOp::Clear;
            colorAttachment.mStoreOp    = Rhi::ERhiStoreOp::Store;
            colorAttachment.mClearColor = Rhi::FRhiClearColor{ 0.08f, 0.08f, 0.12f, 1.0f };

            Rhi::FRhiRenderPassDesc passDesc{};
            passDesc.mColorAttachmentCount = 1U;
            passDesc.mColorAttachments     = &colorAttachment;
            mCmdList.Emplace<Rhi::FRhiCmdBeginRenderPass>(passDesc);
            mCmdList.Emplace<Rhi::FRhiCmdSetPrimitiveTopology>(
                Rhi::ERhiPrimitiveTopology::TriangleList);
            mCmdList.Emplace<Rhi::FRhiCmdSetViewport>(Rhi::FRhiViewportRect{
                0.0f, 0.0f, static_cast<f32>(width), static_cast<f32>(height), 0.0f, 1.0f });
            mCmdList.Emplace<Rhi::FRhiCmdSetScissor>(Rhi::FRhiScissorRect{ 0, 0, width, height });
            mCmdList.Emplace<Rhi::FRhiCmdSetIndexBuffer>(
                Rhi::FRhiIndexBufferView{ mIndexBuffer.Get(), Rhi::ERhiIndexType::Uint16, 0U });
            mCmdList.Emplace<Rhi::FRhiCmdDrawIndexed>(3U, 1U, 0U, 0, 0U);
            mCmdList.Emplace<Rhi::FRhiCmdEndRenderPass>();

            Rhi::FRhiCmdExecutor::Execute(mCmdList, adapter);
            adapter.End();

            auto* rhiCommandList = d3dContext->GetCommandList();
            if (rhiCommandList == nullptr) {
                return;
            }

            Rhi::FRhiCommandList* commandLists[] = { rhiCommandList };
            Rhi::FRhiSubmitInfo   submit{};
            submit.mCommandLists     = commandLists;
            submit.mCommandListCount = 1U;
            mQueue->Submit(submit);
#else
            (void)device;
            (void)viewport;
            (void)width;
            (void)height;
#endif
        }

        void Shutdown() {
            mCmdList.Reset();
            mQueue.Reset();
            mCommandContext.Reset();
            mIndexBuffer.Reset();
            mPipeline.Reset();
            mPixelShader.Reset();
            mVertexShader.Reset();
            mInitialized = false;
        }

    private:
        bool Initialize(Rhi::FRhiDevice& device) {
#if AE_PLATFORM_WIN
            auto BuildShader = [&](const char* entryPoint, Shader::EShaderStage stage,
                                   const char* targetProfile, const char* label,
                                   Rhi::FRhiShaderRef& outShader) -> bool {
                Shader::FShaderBytecode   bytecode;
                Shader::FShaderReflection reflection;
                Container::FNativeString  errors;

                if (ShaderCompileHelpers::CompileD3D11ShaderWithShaderCompiler(kTriangleShaderHlsl,
                        entryPoint, stage, ShaderCompiler::EShaderSourceLanguage::Hlsl, bytecode,
                        reflection, errors, targetProfile)) {
                    outShader = ShaderCompileHelpers::CreateShaderFromBytecode(device, stage,
                        AltinaEngine::Move(bytecode), AltinaEngine::Move(reflection));
                    if (outShader) {
                        return true;
                    }
                    std::cerr << "[Triangle] " << label
                              << " create failed for DXC output; trying Slang.\n";
                } else if (!errors.IsEmptyString()) {
                    std::cerr << "[Triangle] " << label << " DXC compile failed:\n"
                              << errors.CStr() << "\n";
                }

                errors.Clear();
                if (ShaderCompileHelpers::CompileD3D11ShaderWithShaderCompiler(kTriangleShaderHlsl,
                        entryPoint, stage, ShaderCompiler::EShaderSourceLanguage::Slang, bytecode,
                        reflection, errors, targetProfile)) {
                    outShader = ShaderCompileHelpers::CreateShaderFromBytecode(device, stage,
                        AltinaEngine::Move(bytecode), AltinaEngine::Move(reflection));
                    if (outShader) {
                        return true;
                    }
                    std::cerr << "[Triangle] " << label
                              << " create failed for Slang output; trying D3DCompile.\n";
                } else if (!errors.IsEmptyString()) {
                    std::cerr << "[Triangle] " << label << " Slang compile failed:\n"
                              << errors.CStr() << "\n";
                }

                errors.Clear();
                Shader::FShaderBytecode fxcBytecode;
                if (ShaderCompileHelpers::CompileD3D11ShaderFxc(
                        kTriangleShaderHlsl, entryPoint, targetProfile, fxcBytecode, errors)) {
                    outShader = ShaderCompileHelpers::CreateShaderFromBytecode(
                        device, stage, AltinaEngine::Move(fxcBytecode), {});
                    if (outShader) {
                        std::cerr << "[Triangle] " << label
                                  << " created via D3DCompile fallback.\n";
                        return true;
                    }
                    std::cerr << "[Triangle] " << label << " create failed after D3DCompile.\n";
                }

                if (!errors.IsEmptyString()) {
                    std::cerr << "[Triangle] " << label << " D3DCompile failed:\n"
                              << errors.CStr() << "\n";
                }
                return false;
            };

            if (!BuildShader(
                    "VSMain", Shader::EShaderStage::Vertex, "vs_5_0", "VS", mVertexShader)) {
                return false;
            }

            if (!BuildShader("PSMain", Shader::EShaderStage::Pixel, "ps_5_0", "PS", mPixelShader)) {
                return false;
            }

            Rhi::FRhiGraphicsPipelineDesc pipelineDesc{};
            pipelineDesc.mVertexShader = mVertexShader.Get();
            pipelineDesc.mPixelShader  = mPixelShader.Get();

            mPipeline = device.CreateGraphicsPipeline(pipelineDesc);
            if (!mPipeline) {
                return false;
            }

            Rhi::FRhiBufferDesc indexDesc{};
            indexDesc.mSizeBytes = sizeof(u16) * 3U;
            indexDesc.mUsage     = Rhi::ERhiResourceUsage::Dynamic;
            indexDesc.mBindFlags = Rhi::ERhiBufferBindFlags::Index;
            indexDesc.mCpuAccess = Rhi::ERhiCpuAccess::Write;

            mIndexBuffer = device.CreateBuffer(indexDesc);
            if (!mIndexBuffer) {
                return false;
            }

            const u16 indices[3] = { 0U, 1U, 2U };
            auto      lock       = mIndexBuffer->Lock(
                0ULL, indexDesc.mSizeBytes, Rhi::ERhiBufferLockMode::WriteDiscard);
            if (lock.mData != nullptr) {
                std::memcpy(lock.mData, indices, sizeof(indices));
            }
            mIndexBuffer->Unlock(lock);

            Rhi::FRhiCommandContextDesc ctxDesc{};
            ctxDesc.mQueueType = Rhi::ERhiQueueType::Graphics;
            mCommandContext    = device.CreateCommandContext(ctxDesc);
            mQueue             = device.GetQueue(Rhi::ERhiQueueType::Graphics);

            mInitialized = mCommandContext && mQueue;
            return mInitialized;
#else
            (void)device;
            return false;
#endif
        }

        bool                       mInitialized = false;
        Rhi::FRhiShaderRef         mVertexShader;
        Rhi::FRhiShaderRef         mPixelShader;
        Rhi::FRhiPipelineRef       mPipeline;
        Rhi::FRhiBufferRef         mIndexBuffer;
        Rhi::FRhiCommandContextRef mCommandContext;
        Rhi::FRhiQueueRef          mQueue;
        Rhi::FRhiCmdList           mCmdList;
    };
} // namespace

int main(int argc, char** argv) {
    Reflection::RegisterType<Neko>();
    Reflection::RegisterPropertyField<&Neko::mMeow>("Meow");
    Reflection::RegisterPropertyField<&Neko::mNya>("Nya");

    auto                nyaMeta = TypeMeta::FMetaPropertyInfo::Create<&Neko::mNya>();
    Reflection::FObject obj = Reflection::ConstructObject(TypeMeta::FMetaTypeInfo::Create<Neko>());
    auto                propObj = Reflection::GetProperty(obj, nyaMeta);
    auto&               nyaRef  = propObj.As<Container::TRef<int>>().Get();
    nyaRef                      = 514;

    auto& p = obj.As<Neko>();
    LogError(TEXT("Neko mNya value after reflection set: {}"), p.mNya);

    LogWarning(TEXT("Address for &(p.Nya) and &nyaRef: {}, {}"), (u64) & (p.mNya), (u64)&nyaRef);

    Gameplay::FGameplayModule::ValidateReflection();

    FStartupParameters StartupParams{};
    if (argc > 1) {
        StartupParams.mCommandLine = argv[1];
    }

    FEngineLoop EngineLoop(StartupParams);
    if (!EngineLoop.PreInit()) {
        return 1;
    }
    if (!EngineLoop.Init()) {
        EngineLoop.Exit();
        return 1;
    }

    FTriangleRenderer triangleRenderer;
    EngineLoop.SetRenderCallback(
        [&triangleRenderer](Rhi::FRhiDevice& device, Rhi::FRhiViewport& viewport, u32 width,
            u32 height) { triangleRenderer.Render(device, viewport, width, height); });

    constexpr f32 kFixedDeltaTime          = 1.0f / 60.0f;
    constexpr f32 kMoveSpeedUnitsPerSecond = 300.0f;
    f32           positionX                = 0.0f;
    f32           positionY                = 0.0f;
    i32           lastMoveX                = 0;
    i32           lastMoveY                = 0;

    for (i32 FrameIndex = 0; FrameIndex < 600; ++FrameIndex) {
        EngineLoop.Tick(kFixedDeltaTime);

        if (const auto* inputSystem = EngineLoop.GetInputSystem()) {
            i32 moveX = 0;
            i32 moveY = 0;

            if (inputSystem->IsKeyDown(Input::EKey::W)) {
                moveY += 1;
            }
            if (inputSystem->IsKeyDown(Input::EKey::S)) {
                moveY -= 1;
            }
            if (inputSystem->IsKeyDown(Input::EKey::A)) {
                moveX -= 1;
            }
            if (inputSystem->IsKeyDown(Input::EKey::D)) {
                moveX += 1;
            }

            if (moveX != 0 || moveY != 0) {
                positionX += static_cast<f32>(moveX) * kMoveSpeedUnitsPerSecond * kFixedDeltaTime;
                positionY += static_cast<f32>(moveY) * kMoveSpeedUnitsPerSecond * kFixedDeltaTime;
            }

            if (moveX != lastMoveX || moveY != lastMoveY) {
                LogInfo(
                    TEXT("Move input: ({}, {}), pos=({}, {})"), moveX, moveY, positionX, positionY);
                lastMoveX = moveX;
                lastMoveY = moveY;
            }

            if (inputSystem->WasKeyPressed(Input::EKey::Space)) {
                LogInfo(TEXT("Space pressed."));
            }
        }

        AltinaEngine::Core::Platform::Generic::PlatformSleepMilliseconds(16);

        // LogError(TEXT("Frame {} processed."), FrameIndex);
    }

    EngineLoop.SetRenderCallback({});
    triangleRenderer.Shutdown();
    EngineLoop.Exit();
    return 0;
}
