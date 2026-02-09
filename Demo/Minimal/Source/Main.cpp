#include "Base/AltinaBase.h"
#include "Launch/EngineLoop.h"
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
#include "Types/Aliases.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <string>

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
    using Microsoft::WRL::ComPtr;

    auto CompileD3D11Shader(const char* source, const char* entryPoint, const char* targetProfile,
        Shader::FShaderBytecode& outBytecode, std::string& outErrors) -> bool {
        if (!source || !entryPoint || !targetProfile) {
            return false;
        }

        ComPtr<ID3DBlob> bytecode;
        ComPtr<ID3DBlob> errors;
        const UINT       flags = D3DCOMPILE_ENABLE_STRICTNESS;
        const HRESULT    hr    = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr,
                  entryPoint, targetProfile, flags, 0, &bytecode, &errors);

        outErrors.clear();
        if (errors) {
            const auto* data = static_cast<const char*>(errors->GetBufferPointer());
            outErrors.assign(data, data + errors->GetBufferSize());
        }

        if (FAILED(hr) || !bytecode) {
            return false;
        }

        const SIZE_T size = bytecode->GetBufferSize();
        outBytecode.mData.Resize(static_cast<usize>(size));
        std::memcpy(outBytecode.mData.Data(), bytecode->GetBufferPointer(), size);
        return true;
    }
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
            rtvDesc.mTexture = backBuffer;
            auto backBufferRtv = device.CreateRenderTargetView(rtvDesc);

            Rhi::FRhiRenderPassColorAttachment colorAttachment{};
            colorAttachment.mView = backBufferRtv.Get();
            colorAttachment.mLoadOp = Rhi::ERhiLoadOp::Clear;
            colorAttachment.mStoreOp = Rhi::ERhiStoreOp::Store;
            colorAttachment.mClearColor = Rhi::FRhiClearColor{ 0.08f, 0.08f, 0.12f, 1.0f };

            Rhi::FRhiRenderPassDesc passDesc{};
            passDesc.mColorAttachmentCount = 1U;
            passDesc.mColorAttachments = &colorAttachment;
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
            Shader::FShaderBytecode vsBytecode;
            Shader::FShaderBytecode psBytecode;
            std::string             errors;

            if (!CompileD3D11Shader(kTriangleShaderHlsl, "VSMain", "vs_5_0", vsBytecode, errors)) {
                if (!errors.empty()) {
                    std::cerr << "[Triangle] VS compile failed:\n" << errors << "\n";
                }
                return false;
            }

            if (!CompileD3D11Shader(kTriangleShaderHlsl, "PSMain", "ps_5_0", psBytecode, errors)) {
                if (!errors.empty()) {
                    std::cerr << "[Triangle] PS compile failed:\n" << errors << "\n";
                }
                return false;
            }

            Rhi::FRhiShaderDesc vsDesc{};
            vsDesc.mStage    = Shader::EShaderStage::Vertex;
            vsDesc.mBytecode = AltinaEngine::Move(vsBytecode);

            Rhi::FRhiShaderDesc psDesc{};
            psDesc.mStage    = Shader::EShaderStage::Pixel;
            psDesc.mBytecode = AltinaEngine::Move(psBytecode);

            mVertexShader = device.CreateShader(vsDesc);
            mPixelShader  = device.CreateShader(psDesc);
            if (!mVertexShader || !mPixelShader) {
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

    for (i32 FrameIndex = 0; FrameIndex < 600; ++FrameIndex) {
        EngineLoop.Tick(1.0f / 60.0f);
        AltinaEngine::Core::Platform::Generic::PlatformSleepMilliseconds(16);

        // LogError(TEXT("Frame {} processed."), FrameIndex);
    }

    EngineLoop.SetRenderCallback({});
    triangleRenderer.Shutdown();
    EngineLoop.Exit();
    return 0;
}
