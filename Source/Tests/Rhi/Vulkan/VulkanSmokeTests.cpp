#include "TestHarness.h"

#include "RhiVulkan/RhiVulkanContext.h"

#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiCommandContext.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiShader.h"
#include "Rhi/Command/RhiCmdContextOps.h"
#include "Types/CheckedCast.h"

#include "ShaderCompiler/ShaderCompiler.h"
#include "ShaderCompiler/ShaderRhiBindings.h"
#include "Utility/String/CodeConvert.h"

#include <cstring>
#include <cstdlib>

using AltinaEngine::Core::Container::FString;
using AltinaEngine::Rhi::ERhiBackend;
using AltinaEngine::Rhi::ERhiQueueType;
using AltinaEngine::Rhi::FRhiBindGroupLayoutDesc;
using AltinaEngine::Rhi::FRhiCommandContextDesc;
using AltinaEngine::Rhi::FRhiComputePipelineDesc;
using AltinaEngine::Rhi::FRhiDevice;
using AltinaEngine::Rhi::FRhiDeviceDesc;
using AltinaEngine::Rhi::FRhiInitDesc;
using AltinaEngine::Rhi::FRhiPipelineLayoutDesc;
using AltinaEngine::Rhi::FRhiVulkanContext;
using AltinaEngine::Rhi::RHIExit;
using AltinaEngine::Rhi::RHIGetBackend;
using AltinaEngine::Rhi::RHIInit;
using AltinaEngine::ShaderCompiler::FShaderCompileRequest;
using AltinaEngine::ShaderCompiler::FShaderCompileResult;
using AltinaEngine::ShaderCompiler::GetShaderCompiler;

namespace {
    auto MakeShaderPath(const AltinaEngine::TChar* relative) -> FString {
        const size_t len  = std::strlen(AE_SOURCE_DIR);
        FString      path = AltinaEngine::Core::Utility::String::FromUtf8Bytes(
            AE_SOURCE_DIR, static_cast<AltinaEngine::usize>(len));
        path.Append(TEXT("/"));
        path.Append(relative);
        return path;
    }
} // namespace

TEST_CASE("Rhi.Vulkan.Smoke.InitDeviceAndDispatch") {
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
    if (deviceShared) {
        REQUIRE(RHIGetBackend() == ERhiBackend::Vulkan);

        FRhiDevice*           device = deviceShared.Get();

        // Compile a minimal compute shader to SPIR-V via the engine shader compiler.
        FShaderCompileRequest request{};
        request.mSource.mPath =
            MakeShaderPath(TEXT("Source/Tests/Rhi/Vulkan/Shaders/EmptyCS.hlsl"));
        request.mSource.mEntryPoint.Assign(TEXT("main"));
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

        const FShaderCompileResult compiled = GetShaderCompiler().Compile(request);
        if (!compiled.mSucceeded) {
            const auto utf8 =
                AltinaEngine::Core::Utility::String::ToUtf8Bytes(compiled.mDiagnostics);
            std::cerr << "Shader compile failed: " << utf8.CStr() << "\n";
        }
        REQUIRE(compiled.mSucceeded);
        REQUIRE(!compiled.mBytecode.IsEmpty());

        const auto shaderDesc = AltinaEngine::ShaderCompiler::BuildRhiShaderDesc(compiled);
        auto       shader     = device->CreateShader(shaderDesc);
        REQUIRE(shader.Get() != nullptr);

        // Empty layout is valid for a shader with no resources.
        FRhiPipelineLayoutDesc layoutDesc{};
        auto                   layout = device->CreatePipelineLayout(layoutDesc);
        REQUIRE(layout.Get() != nullptr);

        FRhiComputePipelineDesc pipelineDesc{};
        pipelineDesc.mPipelineLayout = layout.Get();
        pipelineDesc.mComputeShader  = shader.Get();
        auto pipeline                = device->CreateComputePipeline(pipelineDesc);
        REQUIRE(pipeline.Get() != nullptr);

        FRhiCommandContextDesc ctxDesc{};
        ctxDesc.mQueueType = ERhiQueueType::Compute;
        auto cmd           = device->CreateCommandContext(ctxDesc);
        REQUIRE(cmd.Get() != nullptr);
        auto* ops = static_cast<AltinaEngine::Rhi::IRhiCmdContextOps*>(cmd.Get());
        REQUIRE(ops != nullptr);

        ops->RHISetComputePipeline(pipeline.Get());
        ops->RHIDispatch(1, 1, 1);
        cmd->RHIFlushContextDevice({});

        auto queue = device->GetQueue(ERhiQueueType::Compute);
        REQUIRE(queue.Get() != nullptr);
        queue->WaitIdle();

        cmd.Reset();
        pipeline.Reset();
        layout.Reset();
        shader.Reset();
        queue.Reset();
    } else {
        // Stub-only builds: Vulkan backend isn't enabled, but the module should remain buildable.
        REQUIRE(deviceShared.Get() == nullptr);
    }

    // Ensure the device is destroyed before the context tears down the VkInstance.
    deviceShared.Reset();
    RHIExit(context);
}
