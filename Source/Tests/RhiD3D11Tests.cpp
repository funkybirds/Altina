#include "TestHarness.h"

#include <cstring>
#include <thread>
#include <chrono>
#include <string>

#include "RhiD3D11/RhiD3D11Context.h"
#include "RhiD3D11/RhiD3D11CommandContext.h"
#include "RhiD3D11/RhiD3D11CommandList.h"
#include "RhiD3D11/RhiD3D11Device.h"
#include "RhiD3D11/RhiD3D11Resources.h"
#include "RhiD3D11/RhiD3D11Shader.h"
#include "Rhi/Command/RhiCmdBuiltins.h"
#include "Rhi/Command/RhiCmdContextAdapter.h"
#include "Rhi/Command/RhiCmdExecutor.h"
#include "Rhi/Command/RhiCmdList.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiShader.h"
#include "Rhi/RhiStructs.h"
#include "Rhi/RhiTexture.h"
#include "Types/Traits.h"

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
    #ifdef CreateSemaphore
        #undef CreateSemaphore
    #endif
    #include <d3d11.h>
    #include <d3dcompiler.h>
    #include <wrl/client.h>
#endif

namespace {
    using AltinaEngine::u32;
    using AltinaEngine::Rhi::ERhiQueueType;
    using AltinaEngine::Rhi::FRhiCommandList;
    using AltinaEngine::Rhi::FRhiCommandContextDesc;
    using AltinaEngine::Rhi::FRhiShaderDesc;
    using AltinaEngine::Rhi::FRhiSubmitInfo;
    using AltinaEngine::Rhi::FRhiD3D11CommandContext;
    using AltinaEngine::Rhi::FRhiD3D11Device;

#if AE_PLATFORM_WIN
    auto CompileD3D11ShaderDXBC(const char* source, const char* entryPoint,
        const char* targetProfile, AltinaEngine::Shader::FShaderBytecode& outBytecode,
        std::string& outErrors) -> bool {
        if (!source || !entryPoint || !targetProfile) {
            return false;
        }

        Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
        Microsoft::WRL::ComPtr<ID3DBlob> errors;
        const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
        const HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr,
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
        outBytecode.mData.Resize(static_cast<AltinaEngine::usize>(size));
        std::memcpy(outBytecode.mData.Data(), bytecode->GetBufferPointer(), size);
        return true;
    }
#endif

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

    constexpr const char* kMinimalCsShader = R"(RWStructuredBuffer<uint> Output : register(u0);

[numthreads(1, 1, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID) {
    Output[0] = 123u;
})";

    constexpr const char* kGraphicsUavPsShader = R"(RWTexture2D<float4> gOut : register(u1);

float4 PSMain() : SV_Target0 {
    gOut[uint2(0, 0)] = float4(1, 0, 0, 1);
    return float4(0, 0, 0, 1);
}
)";
} // namespace

TEST_CASE("RhiD3D11.DeviceCreation") {
#if AE_PLATFORM_WIN
    using AltinaEngine::Rhi::ERhiBufferBindFlags;
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

    AltinaEngine::Shader::FShaderBytecode bytecode;
    std::string compileErrors;
    if (!CompileD3D11ShaderDXBC(kMinimalVsShader, "VSMain", "vs_5_0", bytecode,
            compileErrors)) {
        if (!compileErrors.empty()) {
            std::cerr << "[SKIP] D3D11 D3DCompile failed:\n" << compileErrors << "\n";
        }
        return;
    }

    FRhiShaderDesc shaderDesc;
    shaderDesc.mStage = AltinaEngine::Shader::EShaderStage::Vertex;
    shaderDesc.mBytecode = AltinaEngine::Move(bytecode);

    const auto shader = device->CreateShader(shaderDesc);
    REQUIRE(shader);
#else
    // Non-Windows platforms do not support D3D11.
#endif
}

TEST_CASE("RhiD3D11.DeferredContextSubmitExecutes") {
#if AE_PLATFORM_WIN
    using AltinaEngine::Rhi::FRhiD3D11Context;
    using AltinaEngine::Rhi::FRhiInitDesc;
    using AltinaEngine::Rhi::kRhiInvalidAdapterIndex;
    using Microsoft::WRL::ComPtr;

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

    auto* d3dDevice = static_cast<FRhiD3D11Device*>(device.Get());
    REQUIRE(d3dDevice);
    ID3D11Device* nativeDevice = d3dDevice->GetNativeDevice();
    ID3D11DeviceContext* immediateContext = d3dDevice->GetImmediateContext();
    if (!nativeDevice || !immediateContext) {
        return;
    }

    D3D11_QUERY_DESC queryDesc{};
    queryDesc.Query = D3D11_QUERY_EVENT;
    ComPtr<ID3D11Query> query;
    if (FAILED(nativeDevice->CreateQuery(&queryDesc, &query))) {
        return;
    }

    FRhiCommandContextDesc ctxDesc;
    ctxDesc.mQueueType = ERhiQueueType::Graphics;
    auto cmdContext = device->CreateCommandContext(ctxDesc);
    REQUIRE(cmdContext);

    auto* d3dContext = static_cast<FRhiD3D11CommandContext*>(cmdContext.Get());
    REQUIRE(d3dContext);

    d3dContext->Begin();
    ID3D11DeviceContext* deferredContext = d3dContext->GetDeferredContext();
    if (!deferredContext) {
        return;
    }

    deferredContext->End(query.Get());
    d3dContext->End();

    auto* rhiCommandList = d3dContext->GetCommandList();
    REQUIRE(rhiCommandList);
    FRhiCommandList* commandLists[] = { rhiCommandList };
    FRhiSubmitInfo submit{};
    submit.mCommandLists = commandLists;
    submit.mCommandListCount = 1U;

    auto queue = device->GetQueue(ERhiQueueType::Graphics);
    REQUIRE(queue);
    queue->Submit(submit);

    immediateContext->Flush();

    bool completed = false;
    for (u32 i = 0; i < 200U; ++i) {
        const HRESULT hr = immediateContext->GetData(query.Get(), nullptr, 0, 0);
        if (hr == S_OK) {
            completed = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    REQUIRE(completed);
#else
    // Non-Windows platforms do not support D3D11.
#endif
}

TEST_CASE("RhiD3D11.CmdListAdapterDispatchWrites") {
#if AE_PLATFORM_WIN
    using AltinaEngine::Rhi::FRhiD3D11Context;
    using AltinaEngine::Rhi::FRhiD3D11Shader;
    using AltinaEngine::Rhi::FRhiInitDesc;
    using AltinaEngine::Rhi::FRhiCmdContextAdapter;
    using AltinaEngine::Rhi::FRhiCmdExecutor;
    using AltinaEngine::Rhi::FRhiCmdList;
    using AltinaEngine::Rhi::FRhiCmdDispatch;
    using AltinaEngine::Rhi::kRhiInvalidAdapterIndex;
    using Microsoft::WRL::ComPtr;

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

    auto* d3dDevice = static_cast<FRhiD3D11Device*>(device.Get());
    REQUIRE(d3dDevice);
    ID3D11Device* nativeDevice = d3dDevice->GetNativeDevice();
    ID3D11DeviceContext* immediateContext = d3dDevice->GetImmediateContext();
    if (!nativeDevice || !immediateContext) {
        return;
    }

    AltinaEngine::Shader::FShaderBytecode bytecode;
    std::string compileErrors;
    if (!CompileD3D11ShaderDXBC(kMinimalCsShader, "CSMain", "cs_5_0", bytecode,
            compileErrors)) {
        if (!compileErrors.empty()) {
            std::cerr << "[SKIP] D3D11 D3DCompile failed:\n" << compileErrors << "\n";
        }
        return;
    }

    FRhiShaderDesc shaderDesc;
    shaderDesc.mStage = AltinaEngine::Shader::EShaderStage::Compute;
    shaderDesc.mBytecode = AltinaEngine::Move(bytecode);

    const auto shader = device->CreateShader(shaderDesc);
    REQUIRE(shader);

    auto* d3dShader = static_cast<FRhiD3D11Shader*>(shader.Get());
    ID3D11ComputeShader* computeShader = d3dShader ? d3dShader->GetComputeShader() : nullptr;
    if (!computeShader) {
        return;
    }

    const u32 initialValue = 0U;
    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = sizeof(u32);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = sizeof(u32);

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = &initialValue;

    ComPtr<ID3D11Buffer> buffer;
    if (FAILED(nativeDevice->CreateBuffer(&bufferDesc, &initData, &buffer))) {
        return;
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0U;
    uavDesc.Buffer.NumElements = 1U;
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;

    ComPtr<ID3D11UnorderedAccessView> uav;
    if (FAILED(nativeDevice->CreateUnorderedAccessView(buffer.Get(), &uavDesc, &uav))) {
        return;
    }

    D3D11_BUFFER_DESC stagingDesc = bufferDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0U;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

    ComPtr<ID3D11Buffer> staging;
    if (FAILED(nativeDevice->CreateBuffer(&stagingDesc, nullptr, &staging))) {
        return;
    }

    FRhiCommandContextDesc ctxDesc;
    ctxDesc.mQueueType = ERhiQueueType::Compute;
    auto cmdContext = device->CreateCommandContext(ctxDesc);
    REQUIRE(cmdContext);

    auto* d3dContext = static_cast<FRhiD3D11CommandContext*>(cmdContext.Get());
    REQUIRE(d3dContext);

    FRhiCmdContextAdapter adapter(*d3dContext);
    adapter.Begin();

    ID3D11DeviceContext* deferredContext = d3dContext->GetDeferredContext();
    if (!deferredContext) {
        return;
    }

    deferredContext->CSSetShader(computeShader, nullptr, 0);
    ID3D11UnorderedAccessView* uavs[] = { uav.Get() };
    deferredContext->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

    FRhiCmdList cmdList;
    cmdList.Emplace<FRhiCmdDispatch>(1U, 1U, 1U);
    FRhiCmdExecutor::Execute(cmdList, adapter);

    adapter.End();

    auto* rhiCommandList = d3dContext->GetCommandList();
    REQUIRE(rhiCommandList);
    FRhiCommandList* commandLists[] = { rhiCommandList };
    FRhiSubmitInfo submit{};
    submit.mCommandLists = commandLists;
    submit.mCommandListCount = 1U;

    auto queue = device->GetQueue(ERhiQueueType::Compute);
    REQUIRE(queue);
    queue->Submit(submit);

    ID3D11UnorderedAccessView* nullUavs[] = { nullptr };
    immediateContext->CSSetUnorderedAccessViews(0, 1, nullUavs, nullptr);
    immediateContext->CopyResource(staging.Get(), buffer.Get());
    immediateContext->Flush();

    D3D11_MAPPED_SUBRESOURCE mapped{};
    const HRESULT mapHr = immediateContext->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(mapHr)) {
        return;
    }
    const u32 value = *static_cast<const u32*>(mapped.pData);
    immediateContext->Unmap(staging.Get(), 0);

    REQUIRE_EQ(value, 123U);
#else
    // Non-Windows platforms do not support D3D11.
#endif
}

TEST_CASE("RhiD3D11.GraphicsUavBindingRespectsRtvSlots") {
#if AE_PLATFORM_WIN
    using AltinaEngine::Rhi::ERhiBindingType;
    using AltinaEngine::Rhi::ERhiQueueType;
    using AltinaEngine::Rhi::ERhiShaderStageFlags;
    using AltinaEngine::Rhi::FRhiBindGroupDesc;
    using AltinaEngine::Rhi::FRhiBindGroupEntry;
    using AltinaEngine::Rhi::FRhiBindGroupLayoutDesc;
    using AltinaEngine::Rhi::FRhiBindGroupLayoutEntry;
    using AltinaEngine::Rhi::FRhiCommandContextDesc;
    using AltinaEngine::Rhi::FRhiD3D11Context;
    using AltinaEngine::Rhi::FRhiD3D11Texture;
    using AltinaEngine::Rhi::FRhiD3D11Device;
    using AltinaEngine::Rhi::FRhiGraphicsPipelineDesc;
    using AltinaEngine::Rhi::FRhiInitDesc;
    using AltinaEngine::Rhi::FRhiPipelineLayoutDesc;
    using AltinaEngine::Rhi::FRhiShaderDesc;
    using AltinaEngine::Rhi::FRhiTexture;
    using AltinaEngine::Rhi::FRhiTextureDesc;
    using AltinaEngine::Rhi::kRhiInvalidAdapterIndex;
    using AltinaEngine::Shader::EShaderResourceAccess;
    using AltinaEngine::Shader::EShaderResourceType;
    using AltinaEngine::Shader::EShaderStage;
    using AltinaEngine::Shader::FShaderResourceBinding;
    using Microsoft::WRL::ComPtr;

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

    auto* d3dDevice = static_cast<FRhiD3D11Device*>(device.Get());
    REQUIRE(d3dDevice);
    ID3D11Device* nativeDevice = d3dDevice->GetNativeDevice();
    if (!nativeDevice) {
        return;
    }

    AltinaEngine::Shader::FShaderBytecode vsBytecode;
    AltinaEngine::Shader::FShaderBytecode psBytecode;
    std::string compileErrors;
    if (!CompileD3D11ShaderDXBC(kMinimalVsShader, "VSMain", "vs_5_0", vsBytecode,
            compileErrors)) {
        if (!compileErrors.empty()) {
            std::cerr << "[SKIP] D3D11 D3DCompile failed:\n" << compileErrors << "\n";
        }
        return;
    }
    if (!CompileD3D11ShaderDXBC(kGraphicsUavPsShader, "PSMain", "ps_5_0", psBytecode,
            compileErrors)) {
        if (!compileErrors.empty()) {
            std::cerr << "[SKIP] D3D11 D3DCompile failed:\n" << compileErrors << "\n";
        }
        return;
    }

    FRhiShaderDesc vsDesc;
    vsDesc.mStage    = EShaderStage::Vertex;
    vsDesc.mBytecode = AltinaEngine::Move(vsBytecode);

    FRhiShaderDesc psDesc;
    psDesc.mStage    = EShaderStage::Pixel;
    psDesc.mBytecode = AltinaEngine::Move(psBytecode);

    FShaderResourceBinding uavBinding;
    uavBinding.mType     = EShaderResourceType::StorageTexture;
    uavBinding.mAccess   = EShaderResourceAccess::ReadWrite;
    uavBinding.mSet      = 0U;
    uavBinding.mBinding  = 1U;
    uavBinding.mRegister = 1U;
    uavBinding.mSpace    = 0U;
    psDesc.mReflection.mResources.PushBack(uavBinding);

    const auto vs = device->CreateShader(vsDesc);
    const auto ps = device->CreateShader(psDesc);
    REQUIRE(vs);
    REQUIRE(ps);

    FRhiBindGroupLayoutDesc layoutDesc;
    layoutDesc.mSetIndex = 0U;
    FRhiBindGroupLayoutEntry layoutEntry;
    layoutEntry.mBinding    = 1U;
    layoutEntry.mType       = ERhiBindingType::StorageTexture;
    layoutEntry.mVisibility = ERhiShaderStageFlags::Pixel;
    layoutDesc.mEntries.PushBack(layoutEntry);

    const auto bindGroupLayout = device->CreateBindGroupLayout(layoutDesc);
    REQUIRE(bindGroupLayout);

    FRhiPipelineLayoutDesc pipelineLayoutDesc;
    pipelineLayoutDesc.mBindGroupLayouts.PushBack(bindGroupLayout.Get());
    const auto pipelineLayout = device->CreatePipelineLayout(pipelineLayoutDesc);
    REQUIRE(pipelineLayout);

    FRhiGraphicsPipelineDesc pipelineDesc;
    pipelineDesc.mPipelineLayout = pipelineLayout.Get();
    pipelineDesc.mVertexShader   = vs.Get();
    pipelineDesc.mPixelShader    = ps.Get();
    const auto pipeline = device->CreateGraphicsPipeline(pipelineDesc);
    REQUIRE(pipeline);

    FRhiTextureDesc rtvDesc;
    rtvDesc.mWidth     = 4U;
    rtvDesc.mHeight    = 4U;
    rtvDesc.mBindFlags = AltinaEngine::Rhi::ERhiTextureBindFlags::RenderTarget;
    const auto colorTarget = device->CreateTexture(rtvDesc);
    REQUIRE(colorTarget);

    FRhiTextureDesc uavDesc;
    uavDesc.mWidth     = 4U;
    uavDesc.mHeight    = 4U;
    uavDesc.mBindFlags = AltinaEngine::Rhi::ERhiTextureBindFlags::UnorderedAccess;
    const auto uavTexture = device->CreateTexture(uavDesc);
    REQUIRE(uavTexture);

    FRhiBindGroupDesc bindGroupDesc;
    bindGroupDesc.mLayout = bindGroupLayout.Get();
    FRhiBindGroupEntry bindEntry;
    bindEntry.mBinding = 1U;
    bindEntry.mType    = ERhiBindingType::StorageTexture;
    bindEntry.mTexture = uavTexture.Get();
    bindGroupDesc.mEntries.PushBack(bindEntry);
    const auto bindGroup = device->CreateBindGroup(bindGroupDesc);
    REQUIRE(bindGroup);

    FRhiCommandContextDesc ctxDesc;
    ctxDesc.mQueueType = ERhiQueueType::Graphics;
    const auto cmdContext = device->CreateCommandContext(ctxDesc);
    REQUIRE(cmdContext);

    auto* d3dContext = static_cast<FRhiD3D11CommandContext*>(cmdContext.Get());
    REQUIRE(d3dContext);
    d3dContext->Begin();

    FRhiTexture* colorTargets[] = { colorTarget.Get() };
    d3dContext->RHISetRenderTargets(1U, colorTargets, nullptr);
    d3dContext->RHISetGraphicsPipeline(pipeline.Get());
    d3dContext->RHISetBindGroup(0U, bindGroup.Get(), nullptr, 0U);

    ID3D11DeviceContext* deferredContext = d3dContext->GetDeferredContext();
    if (!deferredContext) {
        return;
    }

    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3D11UnorderedAccessView> uav;

    deferredContext->OMGetRenderTargetsAndUnorderedAccessViews(
        1U, rtv.GetAddressOf(), dsv.GetAddressOf(), 1U, 1U, uav.GetAddressOf());

    auto* expectedRtv =
        static_cast<FRhiD3D11Texture*>(colorTarget.Get())->GetRenderTargetView();
    auto* expectedUav =
        static_cast<FRhiD3D11Texture*>(uavTexture.Get())->GetUnorderedAccessView();

    REQUIRE(rtv.Get() == expectedRtv);
    REQUIRE(uav.Get() == expectedUav);

    d3dContext->End();
#else
    // Non-Windows platforms do not support D3D11.
#endif
}
