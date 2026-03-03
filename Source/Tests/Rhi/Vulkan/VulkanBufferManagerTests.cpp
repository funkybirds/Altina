#include "TestHarness.h"

#include "RhiVulkan/RhiVulkanContext.h"
#include "RhiVulkan/RhiVulkanDevice.h"
#include "RhiVulkan/RhiVulkanUploadBufferManager.h"
#include "RhiVulkan/RhiVulkanStagingBufferManager.h"

#include "Rhi/RhiInit.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiTexture.h"
#include "Rhi/RhiShader.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiCommandContext.h"
#include "Rhi/Command/RhiCmdContextOps.h"
#include "Types/CheckedCast.h"

#include "ShaderCompiler/ShaderCompiler.h"
#include "ShaderCompiler/ShaderRhiBindings.h"
#include "Utility/String/CodeConvert.h"

#include <cstring>
#include <cstdlib>
#include <iostream>

using AltinaEngine::CheckedCast;
using AltinaEngine::TChar;
using AltinaEngine::u32;
using AltinaEngine::u64;
using AltinaEngine::u8;
using AltinaEngine::Core::Container::FString;
using AltinaEngine::Core::Container::FStringView;
using AltinaEngine::Core::Container::TVector;
using AltinaEngine::Rhi::ERhiBackend;
using AltinaEngine::Rhi::ERhiBindingType;
using AltinaEngine::Rhi::ERhiCpuAccess;
using AltinaEngine::Rhi::ERhiQueueType;
using AltinaEngine::Rhi::ERhiResourceState;
using AltinaEngine::Rhi::ERhiShaderStageFlags;
using AltinaEngine::Rhi::ERhiTextureBindFlags;
using AltinaEngine::Rhi::ERhiTextureDimension;
using AltinaEngine::Rhi::FRhiBindGroupDesc;
using AltinaEngine::Rhi::FRhiBindGroupEntry;
using AltinaEngine::Rhi::FRhiBindGroupLayoutDesc;
using AltinaEngine::Rhi::FRhiBindGroupLayoutEntry;
using AltinaEngine::Rhi::FRhiBufferDesc;
using AltinaEngine::Rhi::FRhiCommandContextDesc;
using AltinaEngine::Rhi::FRhiComputePipelineDesc;
using AltinaEngine::Rhi::FRhiDevice;
using AltinaEngine::Rhi::FRhiDeviceDesc;
using AltinaEngine::Rhi::FRhiInitDesc;
using AltinaEngine::Rhi::FRhiPipelineLayoutDesc;
using AltinaEngine::Rhi::FRhiTextureDesc;
using AltinaEngine::Rhi::FRhiTextureSubresource;
using AltinaEngine::Rhi::FRhiTransitionCreateInfo;
using AltinaEngine::Rhi::FRhiTransitionInfo;
using AltinaEngine::Rhi::FRhiVulkanContext;
using AltinaEngine::Rhi::FRhiVulkanDevice;
using AltinaEngine::Rhi::RHIExit;
using AltinaEngine::Rhi::RHIInit;
using AltinaEngine::Shader::EShaderResourceAccess;
using AltinaEngine::Shader::EShaderResourceType;
using AltinaEngine::ShaderCompiler::BuildRhiShaderDesc;
using AltinaEngine::ShaderCompiler::FShaderCompileRequest;
using AltinaEngine::ShaderCompiler::FShaderCompileResult;
using AltinaEngine::ShaderCompiler::GetShaderCompiler;

namespace {
    auto MakeShaderPath(const TChar* relative) -> FString {
        const size_t len  = std::strlen(AE_SOURCE_DIR);
        FString      path = AltinaEngine::Core::Utility::String::FromUtf8Bytes(
            AE_SOURCE_DIR, static_cast<AltinaEngine::usize>(len));
        path.Append(TEXT("/"));
        path.Append(relative);
        return path;
    }

    auto CompileVulkanShader(const TChar* relativePath, const TChar* entry)
        -> FShaderCompileResult {
        FShaderCompileRequest request{};
        request.mSource.mPath = MakeShaderPath(relativePath);
        request.mSource.mEntryPoint.Assign(entry);
        request.mSource.mStage          = AltinaEngine::Shader::EShaderStage::Compute;
        request.mOptions.mTargetBackend = ERhiBackend::Vulkan;
        request.mOptions.mDebugInfo     = false;

        if (const char* vulkanSdk = std::getenv("VULKAN_SDK")) {
            const size_t sdkLen       = std::strlen(vulkanSdk);
            FString      compilerPath = AltinaEngine::Core::Utility::String::FromUtf8Bytes(
                vulkanSdk, static_cast<AltinaEngine::usize>(sdkLen));
            compilerPath.Append(TEXT("/Bin/slangc.exe"));
            request.mOptions.mCompilerPathOverride = compilerPath;
        }

        return GetShaderCompiler().Compile(request);
    }
} // namespace

TEST_CASE("Rhi.Vulkan.UploadBufferManager.BasicAllocation") {
    FRhiVulkanContext context;

    FRhiInitDesc      init{};
    init.mAppName.Assign(TEXT("AltinaEngineTestsRhiVulkan"));
    init.mBackend             = ERhiBackend::Vulkan;
    init.mEnableDebugLayer    = false;
    init.mEnableGpuValidation = false;
    init.mEnableDebugNames    = false;

    FRhiDeviceDesc deviceDesc{};
    deviceDesc.mEnableDebugLayer    = false;
    deviceDesc.mEnableGpuValidation = false;

    auto deviceShared = RHIInit(context, init, deviceDesc);
    if (!deviceShared) {
        REQUIRE(deviceShared.Get() == nullptr);
        RHIExit(context);
        return;
    }
    auto* device = static_cast<FRhiVulkanDevice*>(deviceShared.Get());
    REQUIRE(device != nullptr);

    AltinaEngine::Rhi::FVulkanUploadBufferManager     manager;
    AltinaEngine::Rhi::FVulkanUploadBufferManagerDesc desc{};
    desc.mPageCount      = 1U;
    desc.mPageSizeBytes  = 1024ULL;
    desc.mAlignmentBytes = 16ULL;
    manager.Init(device, desc);
    manager.BeginFrame(0ULL);

    auto a = manager.Allocate(64ULL, 16ULL, 1ULL);
    auto b = manager.Allocate(32ULL, 16ULL, 2ULL);
    REQUIRE(a.IsValid());
    REQUIRE(b.IsValid());
    REQUIRE(a.mBuffer == b.mBuffer);
    REQUIRE((a.mOffset % 16ULL) == 0ULL);
    REQUIRE((b.mOffset % 16ULL) == 0ULL);
    REQUIRE(a.mOffset != b.mOffset);

    u8 pattern[64]{};
    for (u32 i = 0; i < 64U; ++i) {
        pattern[i] = static_cast<u8>(i);
    }
    REQUIRE(manager.Write(a, pattern, 64ULL, 0ULL));

    void* ptr = manager.GetWritePointer(a, 0ULL);
    REQUIRE(ptr != nullptr);
    REQUIRE(std::memcmp(ptr, pattern, 64) == 0);

    manager.EndFrame();
    manager.Reset();

    deviceShared.Reset();
    RHIExit(context);
}

TEST_CASE("Rhi.Vulkan.StagingBufferManager.PoolReuseAndMap") {
    FRhiVulkanContext context;

    FRhiInitDesc      init{};
    init.mAppName.Assign(TEXT("AltinaEngineTestsRhiVulkan"));
    init.mBackend             = ERhiBackend::Vulkan;
    init.mEnableDebugLayer    = false;
    init.mEnableGpuValidation = false;
    init.mEnableDebugNames    = false;

    FRhiDeviceDesc deviceDesc{};
    deviceDesc.mEnableDebugLayer    = false;
    deviceDesc.mEnableGpuValidation = false;

    auto deviceShared = RHIInit(context, init, deviceDesc);
    if (!deviceShared) {
        REQUIRE(deviceShared.Get() == nullptr);
        RHIExit(context);
        return;
    }

    auto* device = static_cast<FRhiVulkanDevice*>(deviceShared.Get());
    REQUIRE(device != nullptr);
    AltinaEngine::Rhi::FVulkanStagingBufferManager manager;
    manager.Init(device);

    const auto access = ERhiCpuAccess::Read | ERhiCpuAccess::Write;
    auto       a      = manager.Acquire(256ULL, access);
    REQUIRE(a.IsValid());

    void* mapped = manager.Map(a, AltinaEngine::Rhi::EVulkanStagingMapMode::ReadWrite);
    REQUIRE(mapped != nullptr);
    std::memset(mapped, 0xAB, 256);
    manager.Unmap(a);

    void* mapped2 = manager.Map(a, AltinaEngine::Rhi::EVulkanStagingMapMode::Read);
    REQUIRE(mapped2 != nullptr);
    const u8* bytes = static_cast<const u8*>(mapped2);
    REQUIRE(bytes[0] == 0xAB);
    REQUIRE(bytes[255] == 0xAB);
    manager.Unmap(a);

    manager.Release(a);
    auto b = manager.Acquire(128ULL, access);
    REQUIRE(b.IsValid());
    REQUIRE(b.mBuffer == a.mBuffer);
    manager.Release(b);
    manager.Shutdown();

    deviceShared.Reset();
    RHIExit(context);
}

TEST_CASE("Rhi.Vulkan.UpdateTextureSubresource.DispatchReadsCorrectTexel") {
    FRhiVulkanContext context;

    FRhiInitDesc      init{};
    init.mAppName.Assign(TEXT("AltinaEngineTestsRhiVulkan"));
    init.mBackend             = ERhiBackend::Vulkan;
    init.mEnableDebugLayer    = false;
    init.mEnableGpuValidation = false;
    init.mEnableDebugNames    = false;

    FRhiDeviceDesc deviceDesc{};
    deviceDesc.mEnableDebugLayer    = false;
    deviceDesc.mEnableGpuValidation = false;

    auto deviceShared = RHIInit(context, init, deviceDesc);
    if (!deviceShared) {
        REQUIRE(deviceShared.Get() == nullptr);
        RHIExit(context);
        return;
    }

    FRhiDevice* device = deviceShared.Get();
    REQUIRE(device != nullptr);

    const FShaderCompileResult compiled = CompileVulkanShader(
        TEXT("Source/Tests/Rhi/Vulkan/Shaders/TextureReadbackCS.hlsl"), TEXT("main"));
    if (!compiled.mSucceeded) {
        const auto utf8 = AltinaEngine::Core::Utility::String::ToUtf8Bytes(compiled.mDiagnostics);
        std::cerr << "Shader compile failed: " << utf8.CStr() << "\n";
    }
    REQUIRE(compiled.mSucceeded);
    REQUIRE(!compiled.mBytecode.IsEmpty());
    if (!compiled.mSucceeded || compiled.mBytecode.IsEmpty()) {
        deviceShared.Reset();
        RHIExit(context);
        return;
    }

    auto shader = device->CreateShader(BuildRhiShaderDesc(compiled));
    REQUIRE(shader.Get() != nullptr);
    if (!shader) {
        deviceShared.Reset();
        RHIExit(context);
        return;
    }

    // Build bind group layouts from shader compiler output. Vulkan binding numbers may be shifted
    // by the shader compiler options, so hardcoding bindings in tests is fragile.
    TVector<AltinaEngine::Rhi::FRhiBindGroupLayoutRef> bindGroupLayouts;
    bindGroupLayouts.Reserve(compiled.mRhiLayout.mBindGroupLayouts.Size());

    FRhiPipelineLayoutDesc layoutDesc{};
    u32                    maxSet = 0U;
    for (const auto& setDesc : compiled.mRhiLayout.mBindGroupLayouts) {
        if (setDesc.mSetIndex > maxSet) {
            maxSet = setDesc.mSetIndex;
        }
    }
    layoutDesc.mBindGroupLayouts.Resize(maxSet + 1U);

    for (const auto& setDesc : compiled.mRhiLayout.mBindGroupLayouts) {
        auto layout = device->CreateBindGroupLayout(setDesc);
        REQUIRE(layout.Get() != nullptr);
        if (!layout) {
            deviceShared.Reset();
            RHIExit(context);
            return;
        }
        bindGroupLayouts.PushBack(layout);
        layoutDesc.mBindGroupLayouts[setDesc.mSetIndex] = layout.Get();
    }

    // Copy push constant ranges, if any.
    layoutDesc.mPushConstants = compiled.mRhiLayout.mPipelineLayout.mPushConstants;

    auto pipelineLayout = device->CreatePipelineLayout(layoutDesc);
    REQUIRE(pipelineLayout.Get() != nullptr);
    if (!pipelineLayout) {
        deviceShared.Reset();
        RHIExit(context);
        return;
    }

    FRhiComputePipelineDesc pso{};
    pso.mPipelineLayout = pipelineLayout.Get();
    pso.mComputeShader  = shader.Get();
    auto pipeline       = device->CreateComputePipeline(pso);
    REQUIRE(pipeline.Get() != nullptr);
    if (!pipeline) {
        deviceShared.Reset();
        RHIExit(context);
        return;
    }

    FRhiTextureDesc texDesc{};
    texDesc.mDimension   = ERhiTextureDimension::Tex2D;
    texDesc.mWidth       = 4U;
    texDesc.mHeight      = 4U;
    texDesc.mDepth       = 1U;
    texDesc.mArrayLayers = 1U;
    texDesc.mMipLevels   = 1U;
    texDesc.mFormat      = AltinaEngine::Rhi::ERhiFormat::R8G8B8A8Unorm;
    texDesc.mBindFlags   = ERhiTextureBindFlags::ShaderResource | ERhiTextureBindFlags::CopyDst;
    auto texture         = device->CreateTexture(texDesc);
    REQUIRE(texture.Get() != nullptr);
    if (!texture) {
        deviceShared.Reset();
        RHIExit(context);
        return;
    }

    // Upload: first pixel = (255,0,0,255) -> shader sees (1,0,0,1).
    u8 pixels[4 * 4 * 4]{};
    pixels[0] = 255;
    pixels[1] = 0;
    pixels[2] = 0;
    pixels[3] = 255;

    FRhiTextureSubresource sub{};
    sub.mMipLevel   = 0U;
    sub.mArrayLayer = 0U;
    sub.mDepthSlice = 0U;
    device->UpdateTextureSubresource(texture.Get(), sub, pixels, 16U, 0U);

    FRhiBufferDesc outDesc{};
    outDesc.mSizeBytes = 16ULL;
    outDesc.mUsage     = AltinaEngine::Rhi::ERhiResourceUsage::Staging;
    outDesc.mCpuAccess = ERhiCpuAccess::Read | ERhiCpuAccess::Write;
    outDesc.mBindFlags = AltinaEngine::Rhi::ERhiBufferBindFlags::UnorderedAccess
        | AltinaEngine::Rhi::ERhiBufferBindFlags::CopyDst
        | AltinaEngine::Rhi::ERhiBufferBindFlags::CopySrc;
    auto outBuffer = device->CreateBuffer(outDesc);
    REQUIRE(outBuffer.Get() != nullptr);
    if (!outBuffer) {
        deviceShared.Reset();
        RHIExit(context);
        return;
    }

    {
        auto lock =
            outBuffer->Lock(0ULL, 16ULL, AltinaEngine::Rhi::ERhiBufferLockMode::WriteDiscard);
        REQUIRE(lock.IsValid());
        std::memset(lock.mData, 0, 16);
        outBuffer->Unlock(lock);
    }

    // Find the final (possibly shifted) binding numbers from reflection.
    u32 texSet     = 0U;
    u32 texBinding = UINT32_MAX;
    u32 outSet     = 0U;
    u32 outBinding = UINT32_MAX;
    for (const auto& res : compiled.mReflection.mResources) {
        if (res.mName.ToView() == FStringView(TEXT("gTex"))
            && res.mType == EShaderResourceType::Texture) {
            texSet     = res.mSet;
            texBinding = res.mBinding;
        } else if (res.mName.ToView() == FStringView(TEXT("gOut"))
            && res.mType == EShaderResourceType::StorageBuffer
            && res.mAccess == EShaderResourceAccess::ReadWrite) {
            outSet     = res.mSet;
            outBinding = res.mBinding;
        }
    }
    REQUIRE(texBinding != UINT32_MAX);
    REQUIRE(outBinding != UINT32_MAX);
    REQUIRE(texSet == outSet);
    if (texBinding == UINT32_MAX || outBinding == UINT32_MAX || texSet != outSet
        || texSet >= static_cast<u32>(layoutDesc.mBindGroupLayouts.Size())
        || layoutDesc.mBindGroupLayouts[texSet] == nullptr) {
        std::cerr << "Shader resource bindings:\n";
        for (const auto& res : compiled.mReflection.mResources) {
            const auto utf8 = AltinaEngine::Core::Utility::String::ToUtf8Bytes(res.mName);
            std::cerr << "  name=" << utf8.CStr() << " set=" << res.mSet
                      << " binding=" << res.mBinding << " reg=" << res.mRegister
                      << " space=" << res.mSpace << " type=" << static_cast<u32>(res.mType)
                      << " access=" << static_cast<u32>(res.mAccess) << "\n";
        }
        deviceShared.Reset();
        RHIExit(context);
        return;
    }

    FRhiBindGroupDesc groupDesc{};
    groupDesc.mLayout = layoutDesc.mBindGroupLayouts[texSet];
    groupDesc.mEntries.PushBack(FRhiBindGroupEntry{ .mBinding = texBinding,
        .mType                                                = ERhiBindingType::SampledTexture,
        .mTexture                                             = texture.Get() });
    groupDesc.mEntries.PushBack(FRhiBindGroupEntry{ .mBinding = outBinding,
        .mType                                                = ERhiBindingType::StorageBuffer,
        .mBuffer                                              = outBuffer.Get(),
        .mOffset                                              = 0ULL,
        .mSize                                                = 0ULL });
    auto group = device->CreateBindGroup(groupDesc);
    REQUIRE(group.Get() != nullptr);
    if (!group) {
        deviceShared.Reset();
        RHIExit(context);
        return;
    }

    FRhiCommandContextDesc ctxDesc{};
    ctxDesc.mQueueType = ERhiQueueType::Compute;
    auto cmd           = device->CreateCommandContext(ctxDesc);
    REQUIRE(cmd.Get() != nullptr);
    if (!cmd) {
        deviceShared.Reset();
        RHIExit(context);
        return;
    }
    auto* ops = static_cast<AltinaEngine::Rhi::IRhiCmdContextOps*>(cmd.Get());
    REQUIRE(ops != nullptr);
    if (!ops) {
        deviceShared.Reset();
        RHIExit(context);
        return;
    }

    FRhiTransitionInfo transitions[2]{};
    transitions[0].mResource = texture.Get();
    transitions[0].mBefore   = ERhiResourceState::Common;
    transitions[0].mAfter    = ERhiResourceState::ShaderResource;
    transitions[1].mResource = outBuffer.Get();
    transitions[1].mBefore   = ERhiResourceState::Common;
    transitions[1].mAfter    = ERhiResourceState::UnorderedAccess;

    FRhiTransitionCreateInfo tr{};
    tr.mTransitions     = transitions;
    tr.mTransitionCount = 2U;
    tr.mSrcQueue        = ERhiQueueType::Compute;
    tr.mDstQueue        = ERhiQueueType::Compute;

    ops->RHIBeginTransition(tr);
    ops->RHISetComputePipeline(pipeline.Get());
    ops->RHISetBindGroup(texSet, group.Get(), nullptr, 0U);
    ops->RHIDispatch(1U, 1U, 1U);
    cmd->RHIFlushContextDevice({});

    auto queue = device->GetQueue(ERhiQueueType::Compute);
    REQUIRE(queue.Get() != nullptr);
    if (!queue) {
        deviceShared.Reset();
        RHIExit(context);
        return;
    }
    queue->WaitIdle();

    auto lock = outBuffer->Lock(0ULL, 16ULL, AltinaEngine::Rhi::ERhiBufferLockMode::Read);
    REQUIRE(lock.IsValid());
    u32 out[4]{};
    std::memcpy(out, lock.mData, 16);
    outBuffer->Unlock(lock);

    if (out[0] != 0x3F800000U || out[1] != 0x00000000U || out[2] != 0x00000000U
        || out[3] != 0x3F800000U) {
        std::cerr << "Readback uint4 = {0x" << std::hex << out[0] << ", 0x" << out[1] << ", 0x"
                  << out[2] << ", 0x" << out[3] << "}" << std::dec << "\n";
    }

    REQUIRE_EQ(out[0], 0x3F800000U);
    REQUIRE_EQ(out[1], 0x00000000U);
    REQUIRE_EQ(out[2], 0x00000000U);
    REQUIRE_EQ(out[3], 0x3F800000U);

    cmd.Reset();
    group.Reset();
    pipeline.Reset();
    pipelineLayout.Reset();
    bindGroupLayouts.Clear();
    shader.Reset();
    texture.Reset();
    outBuffer.Reset();
    queue.Reset();

    deviceShared.Reset();
    RHIExit(context);
}
