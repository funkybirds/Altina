#include "PostProcess/PostProcessResources.h"

#include "Logging/Log.h"
#include "Platform/PlatformFileSystem.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiShader.h"
#include "ShaderCompiler/ShaderCompiler.h"
#include "ShaderCompiler/ShaderRhiBindings.h"
#include "Utility/Filesystem/Path.h"
#include "Utility/Filesystem/PathUtils.h"

using AltinaEngine::Move;
namespace AltinaEngine::Rendering::PostProcess::Detail {
    namespace {
        using Core::Container::FString;
        using Core::Container::FStringView;
        using Core::Utility::Filesystem::FPath;
        using ShaderCompiler::EShaderOptimization;
        using ShaderCompiler::EShaderSourceLanguage;
        using ShaderCompiler::FShaderCompileRequest;
        using ShaderCompiler::FShaderCompileResult;
        using ShaderCompiler::GetShaderCompiler;

        constexpr FStringView kPostProcessShaderAssetsRelPath =
            TEXT("Assets/Shader/PostProcess/PostProcess.hlsl");
        constexpr FStringView kPostProcessShaderRelPath =
            TEXT("Shader/PostProcess/PostProcess.hlsl");
        constexpr FStringView kPostProcessShaderSourceRelPath =
            TEXT("Source/Shader/PostProcess/PostProcess.hlsl");

        auto FindBuiltinPostProcessShaderPath() -> FPath {
            const FPath exeDir(Core::Platform::GetExecutableDir());
            if (!exeDir.IsEmpty()) {
                const auto candidateAssets = exeDir / kPostProcessShaderAssetsRelPath;
                if (candidateAssets.Exists()) {
                    return candidateAssets;
                }
                const auto candidateLegacy = exeDir / kPostProcessShaderRelPath;
                if (candidateLegacy.Exists()) {
                    return candidateLegacy;
                }

                const auto exeParent = exeDir.ParentPath();
                if (!exeParent.IsEmpty() && exeParent != exeDir) {
                    const auto parentAssets = exeParent / kPostProcessShaderAssetsRelPath;
                    if (parentAssets.Exists()) {
                        return parentAssets;
                    }
                    const auto parentLegacy = exeParent / kPostProcessShaderRelPath;
                    if (parentLegacy.Exists()) {
                        return parentLegacy;
                    }
                }
            }

            const auto cwd = Core::Utility::Filesystem::GetCurrentWorkingDir();
            if (!cwd.IsEmpty()) {
                const auto candidateSource = cwd / kPostProcessShaderSourceRelPath;
                if (candidateSource.Exists()) {
                    return candidateSource;
                }
                const auto candidateAssets = cwd / kPostProcessShaderAssetsRelPath;
                if (candidateAssets.Exists()) {
                    return candidateAssets;
                }
                const auto candidateLegacy = cwd / kPostProcessShaderRelPath;
                if (candidateLegacy.Exists()) {
                    return candidateLegacy;
                }
            }

            FPath probe = cwd;
            for (u32 i = 0U; i < 6U && !probe.IsEmpty(); ++i) {
                const auto candidate = probe / kPostProcessShaderSourceRelPath;
                if (candidate.Exists()) {
                    return candidate;
                }
                const auto parent = probe.ParentPath();
                if (parent == probe) {
                    break;
                }
                probe = parent;
            }
            return {};
        }

        auto BuildIncludeDir(const FPath& shaderPath) -> FPath {
            // <...>/Source/Shader/PostProcess/PostProcess.hlsl -> include root is <...>/Source
            auto includeDir = shaderPath.ParentPath().ParentPath().ParentPath();
            if (!includeDir.IsEmpty()) {
                return includeDir;
            }
            return shaderPath.ParentPath();
        }

        auto CompileShaderFromFile(const FPath& path, FStringView entry, Shader::EShaderStage stage,
            Rhi::FRhiShaderRef& outShader) -> bool {
            if (path.IsEmpty() || !path.Exists()) {
                LogError(TEXT("PostProcess shader source not found: path='{}' entry='{}' stage={}"),
                    path.GetString().ToView(), entry, static_cast<u32>(stage));
                return false;
            }

            FShaderCompileRequest request{};
            request.mSource.mPath.Assign(path.GetString().ToView());
            request.mSource.mEntryPoint.Assign(entry);
            request.mSource.mStage    = stage;
            request.mSource.mLanguage = EShaderSourceLanguage::Hlsl;

            const auto includeDir = BuildIncludeDir(path);
            if (!includeDir.IsEmpty()) {
                request.mSource.mIncludeDirs.PushBack(includeDir.GetString());
            }

            request.mOptions.mTargetBackend = Rhi::ERhiBackend::DirectX11;
            request.mOptions.mOptimization  = EShaderOptimization::Default;
            request.mOptions.mDebugInfo     = false;

            FShaderCompileResult result = GetShaderCompiler().Compile(request);
            if (!result.mSucceeded) {
                LogError(
                    TEXT(
                        "PostProcess shader compile failed: path='{}' entry='{}' stage={} diag={}"),
                    path.GetString().ToView(), entry, static_cast<u32>(stage),
                    result.mDiagnostics.ToView());
                return false;
            }

            auto* device = Rhi::RHIGetDevice();
            if (!device) {
                LogError(TEXT("RHI device missing for PostProcess shader creation."));
                return false;
            }

            auto shaderDesc = ShaderCompiler::BuildRhiShaderDesc(result);
            shaderDesc.mDebugName.Assign(entry);
            outShader = device->CreateShader(shaderDesc);
            if (!outShader) {
                LogError(TEXT("Failed to create PostProcess RHI shader: entry='{}' stage={}"),
                    entry, static_cast<u32>(stage));
                return false;
            }

            return true;
        }

        void UpdateConstantBuffer(Rhi::FRhiBuffer* buffer, const void* data, u64 sizeBytes) {
            if (buffer == nullptr || data == nullptr || sizeBytes == 0ULL) {
                return;
            }

            auto lock = buffer->Lock(0ULL, sizeBytes, Rhi::ERhiBufferLockMode::WriteDiscard);
            if (!lock.IsValid()) {
                return;
            }
            Core::Platform::Generic::Memcpy(lock.mData, data, static_cast<usize>(sizeBytes));
            buffer->Unlock(lock);
        }

        [[nodiscard]] auto BuildLayoutHash(
            const Core::Container::TVector<Rhi::FRhiBindGroupLayoutEntry>& entries, u32 setIndex)
            -> u64 {
            constexpr u64 kOffset = 1469598103934665603ULL;
            constexpr u64 kPrime  = 1099511628211ULL;
            u64           hash    = kOffset;
            auto          mix     = [&](u64 value) { hash = (hash ^ value) * kPrime; };

            mix(setIndex);
            for (const auto& entry : entries) {
                mix(entry.mBinding);
                mix(static_cast<u64>(entry.mType));
                mix(static_cast<u64>(entry.mVisibility));
                mix(entry.mArrayCount);
                mix(entry.mHasDynamicOffset ? 1ULL : 0ULL);
            }
            return hash;
        }

        auto EnsureLayoutAndSampler(Rhi::FRhiDevice& device, FPostProcessSharedResources& res)
            -> bool {
            if (!res.Layout) {
                Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
                layoutDesc.mDebugName.Assign(TEXT("PostProcess.Layout"));
                layoutDesc.mSetIndex = 0U;

                // b0
                {
                    Rhi::FRhiBindGroupLayoutEntry entry{};
                    entry.mBinding          = 0U;
                    entry.mType             = Rhi::ERhiBindingType::ConstantBuffer;
                    entry.mVisibility       = Rhi::ERhiShaderStageFlags::Pixel;
                    entry.mArrayCount       = 1U;
                    entry.mHasDynamicOffset = false;
                    layoutDesc.mEntries.PushBack(entry);
                }

                // t0
                {
                    Rhi::FRhiBindGroupLayoutEntry entry{};
                    entry.mBinding          = 0U;
                    entry.mType             = Rhi::ERhiBindingType::SampledTexture;
                    entry.mVisibility       = Rhi::ERhiShaderStageFlags::Pixel;
                    entry.mArrayCount       = 1U;
                    entry.mHasDynamicOffset = false;
                    layoutDesc.mEntries.PushBack(entry);
                }

                // s0
                {
                    Rhi::FRhiBindGroupLayoutEntry entry{};
                    entry.mBinding          = 0U;
                    entry.mType             = Rhi::ERhiBindingType::Sampler;
                    entry.mVisibility       = Rhi::ERhiShaderStageFlags::Pixel;
                    entry.mArrayCount       = 1U;
                    entry.mHasDynamicOffset = false;
                    layoutDesc.mEntries.PushBack(entry);
                }

                layoutDesc.mLayoutHash = BuildLayoutHash(layoutDesc.mEntries, layoutDesc.mSetIndex);
                res.Layout             = device.CreateBindGroupLayout(layoutDesc);
                if (!res.Layout) {
                    LogError(TEXT("Failed to create PostProcess bind group layout."));
                    return false;
                }
            }

            if (!res.PipelineLayout) {
                Rhi::FRhiPipelineLayoutDesc layoutDesc{};
                layoutDesc.mDebugName.Assign(TEXT("PostProcess.PipelineLayout"));
                layoutDesc.mBindGroupLayouts.PushBack(res.Layout.Get());
                res.PipelineLayout = device.CreatePipelineLayout(layoutDesc);
                if (!res.PipelineLayout) {
                    LogError(TEXT("Failed to create PostProcess pipeline layout."));
                    return false;
                }
            }

            if (!res.LinearSampler) {
                Rhi::FRhiSamplerDesc samplerDesc{};
                samplerDesc.mDebugName.Assign(TEXT("PostProcess.LinearSampler"));
                res.LinearSampler = Rhi::RHICreateSampler(samplerDesc);
                if (!res.LinearSampler) {
                    LogError(TEXT("Failed to create PostProcess sampler."));
                    return false;
                }
            }

            if (!res.ConstantsBuffer) {
                Rhi::FRhiBufferDesc desc{};
                desc.mDebugName.Assign(TEXT("PostProcess.Constants"));
                desc.mSizeBytes     = sizeof(FPostProcessConstants);
                desc.mUsage         = Rhi::ERhiResourceUsage::Dynamic;
                desc.mBindFlags     = Rhi::ERhiBufferBindFlags::Constant;
                desc.mCpuAccess     = Rhi::ERhiCpuAccess::Write;
                res.ConstantsBuffer = device.CreateBuffer(desc);
                if (!res.ConstantsBuffer) {
                    LogError(TEXT("Failed to create PostProcess constant buffer."));
                    return false;
                }

                // Initialize once to sane defaults.
                FPostProcessConstants defaults{};
                UpdateConstantBuffer(res.ConstantsBuffer.Get(), &defaults, sizeof(defaults));
            }

            return true;
        }

        auto EnsurePipelines(Rhi::FRhiDevice& device, FPostProcessSharedResources& res) -> bool {
            if (!res.FullscreenVS || !res.BlitPS || !res.TonemapPS || !res.FxaaPS) {
                const auto shaderPath = FindBuiltinPostProcessShaderPath();
                if (shaderPath.IsEmpty() || !shaderPath.Exists()) {
                    LogError(TEXT("PostProcess shader not found. Expected '{}' or '{}' or '{}'"),
                        kPostProcessShaderAssetsRelPath, kPostProcessShaderRelPath,
                        kPostProcessShaderSourceRelPath);
                    return false;
                }

                LogInfo(TEXT("PostProcess shader path: '{}'"), shaderPath.GetString().ToView());

                if (!CompileShaderFromFile(shaderPath, TEXT("VSFullScreenTriangle"),
                        Shader::EShaderStage::Vertex, res.FullscreenVS)
                    || !CompileShaderFromFile(
                        shaderPath, TEXT("PSBlit"), Shader::EShaderStage::Pixel, res.BlitPS)
                    || !CompileShaderFromFile(
                        shaderPath, TEXT("PSTonemap"), Shader::EShaderStage::Pixel, res.TonemapPS)
                    || !CompileShaderFromFile(
                        shaderPath, TEXT("PSFxaa"), Shader::EShaderStage::Pixel, res.FxaaPS)) {
                    return false;
                }
            }

            if (!res.BlitPipeline) {
                Rhi::FRhiGraphicsPipelineDesc desc{};
                desc.mDebugName.Assign(TEXT("PostProcess.BlitPipeline"));
                desc.mVertexShader            = res.FullscreenVS.Get();
                desc.mPixelShader             = res.BlitPS.Get();
                desc.mPipelineLayout          = res.PipelineLayout.Get();
                desc.mVertexLayout            = {};
                desc.mRasterState             = {};
                desc.mDepthState              = {};
                desc.mBlendState              = {};
                desc.mRasterState.mCullMode   = Rhi::ERhiRasterCullMode::None;
                desc.mDepthState.mDepthEnable = false;
                desc.mDepthState.mDepthWrite  = false;
                res.BlitPipeline              = device.CreateGraphicsPipeline(desc);
                if (!res.BlitPipeline) {
                    LogError(TEXT("Failed to create PostProcess blit pipeline."));
                    return false;
                }
            }

            if (!res.TonemapPipeline) {
                Rhi::FRhiGraphicsPipelineDesc desc{};
                desc.mDebugName.Assign(TEXT("PostProcess.TonemapPipeline"));
                desc.mVertexShader            = res.FullscreenVS.Get();
                desc.mPixelShader             = res.TonemapPS.Get();
                desc.mPipelineLayout          = res.PipelineLayout.Get();
                desc.mVertexLayout            = {};
                desc.mRasterState             = {};
                desc.mDepthState              = {};
                desc.mBlendState              = {};
                desc.mRasterState.mCullMode   = Rhi::ERhiRasterCullMode::None;
                desc.mDepthState.mDepthEnable = false;
                desc.mDepthState.mDepthWrite  = false;
                res.TonemapPipeline           = device.CreateGraphicsPipeline(desc);
                if (!res.TonemapPipeline) {
                    LogError(TEXT("Failed to create PostProcess tonemap pipeline."));
                    return false;
                }
            }

            if (!res.FxaaPipeline) {
                Rhi::FRhiGraphicsPipelineDesc desc{};
                desc.mDebugName.Assign(TEXT("PostProcess.FxaaPipeline"));
                desc.mVertexShader            = res.FullscreenVS.Get();
                desc.mPixelShader             = res.FxaaPS.Get();
                desc.mPipelineLayout          = res.PipelineLayout.Get();
                desc.mVertexLayout            = {};
                desc.mRasterState             = {};
                desc.mDepthState              = {};
                desc.mBlendState              = {};
                desc.mRasterState.mCullMode   = Rhi::ERhiRasterCullMode::None;
                desc.mDepthState.mDepthEnable = false;
                desc.mDepthState.mDepthWrite  = false;
                res.FxaaPipeline              = device.CreateGraphicsPipeline(desc);
                if (!res.FxaaPipeline) {
                    LogError(TEXT("Failed to create PostProcess fxaa pipeline."));
                    return false;
                }
            }

            return true;
        }
    } // namespace

    auto GetPostProcessSharedResources() -> FPostProcessSharedResources& {
        static FPostProcessSharedResources sResources{};
        return sResources;
    }

    auto EnsurePostProcessSharedResources() -> bool {
        auto* device = Rhi::RHIGetDevice();
        if (!device) {
            LogError(TEXT("RHI device missing for PostProcess resources."));
            return false;
        }

        auto& res = GetPostProcessSharedResources();
        if (!EnsureLayoutAndSampler(*device, res)) {
            return false;
        }
        if (!EnsurePipelines(*device, res)) {
            return false;
        }
        return true;
    }
} // namespace AltinaEngine::Rendering::PostProcess::Detail
