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
#include "Shader/ShaderBindingUtility.h"
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

        constexpr FStringView kPostProcessShaderAssetsRelDir = TEXT("Assets/Shader/PostProcess");
        constexpr FStringView kPostProcessShaderRelDir       = TEXT("Shader/PostProcess");
        constexpr FStringView kPostProcessShaderSourceRelDir = TEXT("Source/Shader/PostProcess");

        // Legacy monolithic file (fallback only).
        constexpr FStringView kPostProcessLegacyShaderAssetsRelPath =
            TEXT("Assets/Shader/PostProcess/PostProcess.hlsl");
        constexpr FStringView kPostProcessLegacyShaderRelPath =
            TEXT("Shader/PostProcess/PostProcess.hlsl");
        constexpr FStringView kPostProcessLegacyShaderSourceRelPath =
            TEXT("Source/Shader/PostProcess/PostProcess.hlsl");

        [[nodiscard]] auto DirHasPostProcessShaders(const FPath& dir) -> bool {
            if (dir.IsEmpty() || !dir.Exists()) {
                return false;
            }
            // Use VS file as the probe; the other files live next to it.
            const auto probe = dir / TEXT("FullscreenTriangle.hlsl");
            return probe.Exists();
        }

        auto FindBuiltinPostProcessShaderDir() -> FPath {
            const FPath exeDir(Core::Platform::GetExecutableDir());
            if (!exeDir.IsEmpty()) {
                const auto candidateAssetsDir = exeDir / kPostProcessShaderAssetsRelDir;
                if (DirHasPostProcessShaders(candidateAssetsDir)) {
                    return candidateAssetsDir;
                }
                const auto candidateLegacyDir = exeDir / kPostProcessShaderRelDir;
                if (DirHasPostProcessShaders(candidateLegacyDir)) {
                    return candidateLegacyDir;
                }

                const auto exeParent = exeDir.ParentPath();
                if (!exeParent.IsEmpty() && exeParent != exeDir) {
                    const auto parentAssetsDir = exeParent / kPostProcessShaderAssetsRelDir;
                    if (DirHasPostProcessShaders(parentAssetsDir)) {
                        return parentAssetsDir;
                    }
                    const auto parentLegacyDir = exeParent / kPostProcessShaderRelDir;
                    if (DirHasPostProcessShaders(parentLegacyDir)) {
                        return parentLegacyDir;
                    }
                }
            }

            const auto cwd = Core::Utility::Filesystem::GetCurrentWorkingDir();
            if (!cwd.IsEmpty()) {
                const auto candidateSourceDir = cwd / kPostProcessShaderSourceRelDir;
                if (DirHasPostProcessShaders(candidateSourceDir)) {
                    return candidateSourceDir;
                }
                const auto candidateAssetsDir = cwd / kPostProcessShaderAssetsRelDir;
                if (DirHasPostProcessShaders(candidateAssetsDir)) {
                    return candidateAssetsDir;
                }
                const auto candidateLegacyDir = cwd / kPostProcessShaderRelDir;
                if (DirHasPostProcessShaders(candidateLegacyDir)) {
                    return candidateLegacyDir;
                }
            }

            FPath probe = cwd;
            for (u32 i = 0U; i < 6U && !probe.IsEmpty(); ++i) {
                const auto candidate = probe / kPostProcessShaderSourceRelDir;
                if (DirHasPostProcessShaders(candidate)) {
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

        auto FindLegacyBuiltinPostProcessShaderPath() -> FPath {
            const FPath exeDir(Core::Platform::GetExecutableDir());
            if (!exeDir.IsEmpty()) {
                const auto candidateAssets = exeDir / kPostProcessLegacyShaderAssetsRelPath;
                if (candidateAssets.Exists()) {
                    return candidateAssets;
                }
                const auto candidateLegacy = exeDir / kPostProcessLegacyShaderRelPath;
                if (candidateLegacy.Exists()) {
                    return candidateLegacy;
                }

                const auto exeParent = exeDir.ParentPath();
                if (!exeParent.IsEmpty() && exeParent != exeDir) {
                    const auto parentAssets = exeParent / kPostProcessLegacyShaderAssetsRelPath;
                    if (parentAssets.Exists()) {
                        return parentAssets;
                    }
                    const auto parentLegacy = exeParent / kPostProcessLegacyShaderRelPath;
                    if (parentLegacy.Exists()) {
                        return parentLegacy;
                    }
                }
            }

            const auto cwd = Core::Utility::Filesystem::GetCurrentWorkingDir();
            if (!cwd.IsEmpty()) {
                const auto candidateSource = cwd / kPostProcessLegacyShaderSourceRelPath;
                if (candidateSource.Exists()) {
                    return candidateSource;
                }
                const auto candidateAssets = cwd / kPostProcessLegacyShaderAssetsRelPath;
                if (candidateAssets.Exists()) {
                    return candidateAssets;
                }
                const auto candidateLegacy = cwd / kPostProcessLegacyShaderRelPath;
                if (candidateLegacy.Exists()) {
                    return candidateLegacy;
                }
            }

            FPath probe = cwd;
            for (u32 i = 0U; i < 6U && !probe.IsEmpty(); ++i) {
                const auto candidate = probe / kPostProcessLegacyShaderSourceRelPath;
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
            // <...>/Source/Shader/PostProcess/*.hlsl -> include root is <...>/Source
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

            // Compile for the currently active RHI backend (so RenderDoc captures via Vulkan/D3D
            // see the same passes).
            const auto backend = Rhi::RHIGetBackend();
            request.mOptions.mTargetBackend =
                (backend != Rhi::ERhiBackend::Unknown) ? backend : Rhi::ERhiBackend::DirectX11;
            request.mOptions.mOptimization = EShaderOptimization::Default;
            request.mOptions.mDebugInfo    = false;

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

        auto EnsureShaders(FPostProcessSharedResources& res) -> bool {
            if (!res.FullscreenVS || !res.BlitPS || !res.TonemapPS || !res.FxaaPS
                || !res.BloomPrefilterPS || !res.BloomDownsamplePS || !res.BloomDownsampleWeightedPS
                || !res.BloomBlurHPS || !res.BloomBlurVPS || !res.BloomUpsamplePS
                || !res.BloomApplyPS || !res.TaaPS) {
                const auto shaderDir = FindBuiltinPostProcessShaderDir();
                if (!shaderDir.IsEmpty()) {
                    const auto vsPath      = shaderDir / TEXT("FullscreenTriangle.hlsl");
                    const auto blitPath    = shaderDir / TEXT("Blit.hlsl");
                    const auto tonemapPath = shaderDir / TEXT("Tonemap.hlsl");
                    const auto fxaaPath    = shaderDir / TEXT("Fxaa.hlsl");
                    const auto bloomPath   = shaderDir / TEXT("Bloom.hlsl");
                    const auto taaPath     = shaderDir / TEXT("Taa.hlsl");

                    LogInfo(TEXT("PostProcess shader dir: '{}'"), shaderDir.GetString().ToView());

                    if (!CompileShaderFromFile(vsPath, TEXT("VSFullScreenTriangle"),
                            Shader::EShaderStage::Vertex, res.FullscreenVS)
                        || !CompileShaderFromFile(
                            blitPath, TEXT("PSBlit"), Shader::EShaderStage::Pixel, res.BlitPS)
                        || !CompileShaderFromFile(tonemapPath, TEXT("PSTonemap"),
                            Shader::EShaderStage::Pixel, res.TonemapPS)
                        || !CompileShaderFromFile(
                            fxaaPath, TEXT("PSFxaa"), Shader::EShaderStage::Pixel, res.FxaaPS)
                        || !CompileShaderFromFile(bloomPath, TEXT("PSBloomPrefilter"),
                            Shader::EShaderStage::Pixel, res.BloomPrefilterPS)
                        || !CompileShaderFromFile(bloomPath, TEXT("PSBloomDownsample"),
                            Shader::EShaderStage::Pixel, res.BloomDownsamplePS)
                        || !CompileShaderFromFile(bloomPath, TEXT("PSBloomDownsampleWeighted"),
                            Shader::EShaderStage::Pixel, res.BloomDownsampleWeightedPS)
                        || !CompileShaderFromFile(bloomPath, TEXT("PSBloomBlurH"),
                            Shader::EShaderStage::Pixel, res.BloomBlurHPS)
                        || !CompileShaderFromFile(bloomPath, TEXT("PSBloomBlurV"),
                            Shader::EShaderStage::Pixel, res.BloomBlurVPS)
                        || !CompileShaderFromFile(bloomPath, TEXT("PSBloomUpsample"),
                            Shader::EShaderStage::Pixel, res.BloomUpsamplePS)
                        || !CompileShaderFromFile(bloomPath, TEXT("PSBloomApply"),
                            Shader::EShaderStage::Pixel, res.BloomApplyPS)
                        || !CompileShaderFromFile(
                            taaPath, TEXT("PSTaa"), Shader::EShaderStage::Pixel, res.TaaPS)) {
                        return false;
                    }
                } else {
                    LogError(
                        TEXT("PostProcess shaders not found. Expected dir '{}' or '{}' or '{}'"),
                        kPostProcessShaderAssetsRelDir, kPostProcessShaderRelDir,
                        kPostProcessShaderSourceRelDir);
                    return false;
                }
            }
            return true;
        }

        auto EnsureLayoutAndSampler(Rhi::FRhiDevice& device, FPostProcessSharedResources& res)
            -> bool {
            if (!res.Layout) {
                Core::Container::TVector<Rhi::FRhiShader*> shaders;
                shaders.PushBack(res.BlitPS.Get());
                shaders.PushBack(res.TonemapPS.Get());
                shaders.PushBack(res.FxaaPS.Get());
                shaders.PushBack(res.BloomPrefilterPS.Get());
                shaders.PushBack(res.BloomDownsamplePS.Get());
                shaders.PushBack(res.BloomDownsampleWeightedPS.Get());
                shaders.PushBack(res.BloomBlurHPS.Get());
                shaders.PushBack(res.BloomBlurVPS.Get());
                shaders.PushBack(res.BloomUpsamplePS.Get());
                shaders.PushBack(res.BloomApplyPS.Get());

                Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
                const bool built = RenderCore::ShaderBinding::BuildBindGroupLayoutFromShaders(
                    shaders, 0U, layoutDesc);
                if (!built) {
                    LogError(
                        TEXT("Failed to build PostProcess bind group layout from reflection."));
                    return false;
                }
                layoutDesc.mDebugName.Assign(TEXT("PostProcess.Layout"));
                res.Layout = device.CreateBindGroupLayout(layoutDesc);
                if (!res.Layout) {
                    LogError(TEXT("Failed to create PostProcess bind group layout."));
                    return false;
                }
                if (!RenderCore::ShaderBinding::BuildBindingLookupTableFromShaders(
                        shaders, layoutDesc.mSetIndex, res.Layout.Get(), res.LayoutBindings)) {
                    LogError(TEXT("Failed to build PostProcess layout binding lookup table."));
                    return false;
                }
            }

            // TAA uses a dedicated layout (multiple textures) to avoid breaking the
            // legacy single-texture post-process layout used by other effects.
            if (!res.TaaLayout) {
                Core::Container::TVector<Rhi::FRhiShader*> shaders;
                shaders.PushBack(res.TaaPS.Get());
                Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
                const bool built = RenderCore::ShaderBinding::BuildBindGroupLayoutFromShaders(
                    shaders, 0U, layoutDesc);
                if (!built) {
                    LogError(
                        TEXT("Failed to build PostProcess TAA bind group layout from reflection."));
                    return false;
                }
                layoutDesc.mDebugName.Assign(TEXT("PostProcess.TAA.Layout"));
                res.TaaLayout = device.CreateBindGroupLayout(layoutDesc);
                if (!res.TaaLayout) {
                    LogError(TEXT("Failed to create PostProcess TAA bind group layout."));
                    return false;
                }
                if (!RenderCore::ShaderBinding::BuildBindingLookupTableFromShaders(shaders,
                        layoutDesc.mSetIndex, res.TaaLayout.Get(), res.TaaLayoutBindings)) {
                    LogError(TEXT("Failed to build PostProcess TAA layout binding lookup table."));
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

            if (!res.TaaPipelineLayout) {
                Rhi::FRhiPipelineLayoutDesc layoutDesc{};
                layoutDesc.mDebugName.Assign(TEXT("PostProcess.TAA.PipelineLayout"));
                layoutDesc.mBindGroupLayouts.PushBack(res.TaaLayout.Get());
                res.TaaPipelineLayout = device.CreatePipelineLayout(layoutDesc);
                if (!res.TaaPipelineLayout) {
                    LogError(TEXT("Failed to create PostProcess TAA pipeline layout."));
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

            auto EnsureCB = [&](Rhi::FRhiBufferRef& out, const TChar* debugName, u64 sizeBytes,
                                const void* defaults) -> bool {
                if (out) {
                    return true;
                }
                Rhi::FRhiBufferDesc desc{};
                desc.mDebugName.Assign(debugName);
                desc.mSizeBytes = sizeBytes;
                desc.mUsage     = Rhi::ERhiResourceUsage::Dynamic;
                desc.mBindFlags = Rhi::ERhiBufferBindFlags::Constant;
                desc.mCpuAccess = Rhi::ERhiCpuAccess::Write;
                out             = device.CreateBuffer(desc);
                if (!out) {
                    LogError(TEXT("Failed to create PostProcess constant buffer."));
                    return false;
                }
                UpdateConstantBuffer(out.Get(), defaults, sizeBytes);
                return true;
            };

            // Initialize once to sane defaults (per-pass CBs).
            const FBlitConstants    blitDefaults{};
            const FTonemapConstants tonemapDefaults{};
            const FFxaaConstants    fxaaDefaults{};
            const FBloomConstants   bloomDefaults{};
            const FTaaConstants     taaDefaults{};

            if (!EnsureCB(res.BlitConstantsBuffer, TEXT("PostProcess.Constants.Blit"),
                    sizeof(FBlitConstants), &blitDefaults)
                || !EnsureCB(res.TonemapConstantsBuffer, TEXT("PostProcess.Constants.Tonemap"),
                    sizeof(FTonemapConstants), &tonemapDefaults)
                || !EnsureCB(res.FxaaConstantsBuffer, TEXT("PostProcess.Constants.Fxaa"),
                    sizeof(FFxaaConstants), &fxaaDefaults)
                || !EnsureCB(res.BloomConstantsBuffer, TEXT("PostProcess.Constants.Bloom"),
                    sizeof(FBloomConstants), &bloomDefaults)
                || !EnsureCB(res.TaaConstantsBuffer, TEXT("PostProcess.Constants.TAA"),
                    sizeof(FTaaConstants), &taaDefaults)) {
                return false;
            }

            return true;
        }

        auto EnsurePipelines(Rhi::FRhiDevice& device, FPostProcessSharedResources& res) -> bool {
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

            if (!res.BloomPrefilterPipeline) {
                Rhi::FRhiGraphicsPipelineDesc desc{};
                desc.mDebugName.Assign(TEXT("PostProcess.BloomPrefilterPipeline"));
                desc.mVertexShader            = res.FullscreenVS.Get();
                desc.mPixelShader             = res.BloomPrefilterPS.Get();
                desc.mPipelineLayout          = res.PipelineLayout.Get();
                desc.mVertexLayout            = {};
                desc.mRasterState             = {};
                desc.mDepthState              = {};
                desc.mBlendState              = {};
                desc.mRasterState.mCullMode   = Rhi::ERhiRasterCullMode::None;
                desc.mDepthState.mDepthEnable = false;
                desc.mDepthState.mDepthWrite  = false;
                res.BloomPrefilterPipeline    = device.CreateGraphicsPipeline(desc);
                if (!res.BloomPrefilterPipeline) {
                    LogError(TEXT("Failed to create PostProcess bloom prefilter pipeline."));
                    return false;
                }
            }

            if (!res.BloomDownsamplePipeline) {
                Rhi::FRhiGraphicsPipelineDesc desc{};
                desc.mDebugName.Assign(TEXT("PostProcess.BloomDownsamplePipeline"));
                desc.mVertexShader            = res.FullscreenVS.Get();
                desc.mPixelShader             = res.BloomDownsamplePS.Get();
                desc.mPipelineLayout          = res.PipelineLayout.Get();
                desc.mVertexLayout            = {};
                desc.mRasterState             = {};
                desc.mDepthState              = {};
                desc.mBlendState              = {};
                desc.mRasterState.mCullMode   = Rhi::ERhiRasterCullMode::None;
                desc.mDepthState.mDepthEnable = false;
                desc.mDepthState.mDepthWrite  = false;
                res.BloomDownsamplePipeline   = device.CreateGraphicsPipeline(desc);
                if (!res.BloomDownsamplePipeline) {
                    LogError(TEXT("Failed to create PostProcess bloom downsample pipeline."));
                    return false;
                }
            }

            if (!res.BloomDownsampleWeightedPipeline) {
                Rhi::FRhiGraphicsPipelineDesc desc{};
                desc.mDebugName.Assign(TEXT("PostProcess.BloomDownsampleWeightedPipeline"));
                desc.mVertexShader                  = res.FullscreenVS.Get();
                desc.mPixelShader                   = res.BloomDownsampleWeightedPS.Get();
                desc.mPipelineLayout                = res.PipelineLayout.Get();
                desc.mVertexLayout                  = {};
                desc.mRasterState                   = {};
                desc.mDepthState                    = {};
                desc.mBlendState                    = {};
                desc.mRasterState.mCullMode         = Rhi::ERhiRasterCullMode::None;
                desc.mDepthState.mDepthEnable       = false;
                desc.mDepthState.mDepthWrite        = false;
                res.BloomDownsampleWeightedPipeline = device.CreateGraphicsPipeline(desc);
                if (!res.BloomDownsampleWeightedPipeline) {
                    LogError(
                        TEXT("Failed to create PostProcess bloom downsample-weighted pipeline."));
                    return false;
                }
            }

            if (!res.BloomBlurHPipeline) {
                Rhi::FRhiGraphicsPipelineDesc desc{};
                desc.mDebugName.Assign(TEXT("PostProcess.BloomBlurHPipeline"));
                desc.mVertexShader            = res.FullscreenVS.Get();
                desc.mPixelShader             = res.BloomBlurHPS.Get();
                desc.mPipelineLayout          = res.PipelineLayout.Get();
                desc.mVertexLayout            = {};
                desc.mRasterState             = {};
                desc.mDepthState              = {};
                desc.mBlendState              = {};
                desc.mRasterState.mCullMode   = Rhi::ERhiRasterCullMode::None;
                desc.mDepthState.mDepthEnable = false;
                desc.mDepthState.mDepthWrite  = false;
                res.BloomBlurHPipeline        = device.CreateGraphicsPipeline(desc);
                if (!res.BloomBlurHPipeline) {
                    LogError(TEXT("Failed to create PostProcess bloom blur-h pipeline."));
                    return false;
                }
            }

            if (!res.BloomBlurVPipeline) {
                Rhi::FRhiGraphicsPipelineDesc desc{};
                desc.mDebugName.Assign(TEXT("PostProcess.BloomBlurVPipeline"));
                desc.mVertexShader            = res.FullscreenVS.Get();
                desc.mPixelShader             = res.BloomBlurVPS.Get();
                desc.mPipelineLayout          = res.PipelineLayout.Get();
                desc.mVertexLayout            = {};
                desc.mRasterState             = {};
                desc.mDepthState              = {};
                desc.mBlendState              = {};
                desc.mRasterState.mCullMode   = Rhi::ERhiRasterCullMode::None;
                desc.mDepthState.mDepthEnable = false;
                desc.mDepthState.mDepthWrite  = false;
                res.BloomBlurVPipeline        = device.CreateGraphicsPipeline(desc);
                if (!res.BloomBlurVPipeline) {
                    LogError(TEXT("Failed to create PostProcess bloom blur-v pipeline."));
                    return false;
                }
            }

            if (!res.BloomUpsampleAddPipeline) {
                Rhi::FRhiGraphicsPipelineDesc desc{};
                desc.mDebugName.Assign(TEXT("PostProcess.BloomUpsampleAddPipeline"));
                desc.mVertexShader            = res.FullscreenVS.Get();
                desc.mPixelShader             = res.BloomUpsamplePS.Get();
                desc.mPipelineLayout          = res.PipelineLayout.Get();
                desc.mVertexLayout            = {};
                desc.mRasterState             = {};
                desc.mDepthState              = {};
                desc.mBlendState              = {};
                desc.mRasterState.mCullMode   = Rhi::ERhiRasterCullMode::None;
                desc.mDepthState.mDepthEnable = false;
                desc.mDepthState.mDepthWrite  = false;
                desc.mBlendState.mBlendEnable = true;
                desc.mBlendState.mSrcColor    = Rhi::ERhiBlendFactor::One;
                desc.mBlendState.mDstColor    = Rhi::ERhiBlendFactor::One;
                desc.mBlendState.mColorOp     = Rhi::ERhiBlendOp::Add;
                desc.mBlendState.mSrcAlpha    = Rhi::ERhiBlendFactor::One;
                desc.mBlendState.mDstAlpha    = Rhi::ERhiBlendFactor::One;
                desc.mBlendState.mAlphaOp     = Rhi::ERhiBlendOp::Add;
                res.BloomUpsampleAddPipeline  = device.CreateGraphicsPipeline(desc);
                if (!res.BloomUpsampleAddPipeline) {
                    LogError(TEXT("Failed to create PostProcess bloom upsample-add pipeline."));
                    return false;
                }
            }

            if (!res.BloomApplyAddPipeline) {
                Rhi::FRhiGraphicsPipelineDesc desc{};
                desc.mDebugName.Assign(TEXT("PostProcess.BloomApplyAddPipeline"));
                desc.mVertexShader            = res.FullscreenVS.Get();
                desc.mPixelShader             = res.BloomApplyPS.Get();
                desc.mPipelineLayout          = res.PipelineLayout.Get();
                desc.mVertexLayout            = {};
                desc.mRasterState             = {};
                desc.mDepthState              = {};
                desc.mBlendState              = {};
                desc.mRasterState.mCullMode   = Rhi::ERhiRasterCullMode::None;
                desc.mDepthState.mDepthEnable = false;
                desc.mDepthState.mDepthWrite  = false;
                desc.mBlendState.mBlendEnable = true;
                desc.mBlendState.mSrcColor    = Rhi::ERhiBlendFactor::One;
                desc.mBlendState.mDstColor    = Rhi::ERhiBlendFactor::One;
                desc.mBlendState.mColorOp     = Rhi::ERhiBlendOp::Add;
                desc.mBlendState.mSrcAlpha    = Rhi::ERhiBlendFactor::One;
                desc.mBlendState.mDstAlpha    = Rhi::ERhiBlendFactor::One;
                desc.mBlendState.mAlphaOp     = Rhi::ERhiBlendOp::Add;
                res.BloomApplyAddPipeline     = device.CreateGraphicsPipeline(desc);
                if (!res.BloomApplyAddPipeline) {
                    LogError(TEXT("Failed to create PostProcess bloom apply pipeline."));
                    return false;
                }
            }

            if (!res.TaaPipeline) {
                Rhi::FRhiGraphicsPipelineDesc desc{};
                desc.mDebugName.Assign(TEXT("PostProcess.TAAPipeline"));
                desc.mVertexShader            = res.FullscreenVS.Get();
                desc.mPixelShader             = res.TaaPS.Get();
                desc.mPipelineLayout          = res.TaaPipelineLayout.Get();
                desc.mVertexLayout            = {};
                desc.mRasterState             = {};
                desc.mDepthState              = {};
                desc.mBlendState              = {};
                desc.mRasterState.mCullMode   = Rhi::ERhiRasterCullMode::None;
                desc.mDepthState.mDepthEnable = false;
                desc.mDepthState.mDepthWrite  = false;
                res.TaaPipeline               = device.CreateGraphicsPipeline(desc);
                if (!res.TaaPipeline) {
                    LogError(TEXT("Failed to create PostProcess TAA pipeline."));
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
        if (!EnsureShaders(res)) {
            return false;
        }
        if (!EnsureLayoutAndSampler(*device, res)) {
            return false;
        }
        if (!EnsurePipelines(*device, res)) {
            return false;
        }
        return true;
    }

    auto BuildCommonBindGroupDesc(const FPostProcessSharedResources& resources,
        const TChar* cbufferName, Rhi::FRhiBuffer* cbuffer, u64 cbufferSize,
        Rhi::FRhiTexture* texture, Rhi::FRhiBindGroupDesc& outDesc) -> bool {
        if (!resources.Layout || !resources.LinearSampler || cbuffer == nullptr
            || texture == nullptr || cbufferName == nullptr || cbufferName[0] == 0) {
            return false;
        }

        auto findBinding = [&](const TChar* name, Rhi::ERhiBindingType type,
                               u32& outBinding) -> bool {
            const auto nameHash =
                RenderCore::ShaderBinding::HashBindingName(Core::Container::FStringView(name));
            return RenderCore::ShaderBinding::FindBindingByNameHash(
                resources.LayoutBindings, nameHash, type, outBinding);
        };

        u32 cbBinding      = RenderCore::ShaderBinding::kInvalidBinding;
        u32 texBinding     = RenderCore::ShaderBinding::kInvalidBinding;
        u32 samplerBinding = RenderCore::ShaderBinding::kInvalidBinding;
        if (!findBinding(cbufferName, Rhi::ERhiBindingType::ConstantBuffer, cbBinding)
            || !findBinding(kNameSceneColor, Rhi::ERhiBindingType::SampledTexture, texBinding)
            || !findBinding(kNameLinearSampler, Rhi::ERhiBindingType::Sampler, samplerBinding)) {
            return false;
        }

        RenderCore::ShaderBinding::FBindGroupBuilder builder(resources.Layout.Get());
        if (!builder.AddBuffer(cbBinding, cbuffer, 0ULL, cbufferSize)
            || !builder.AddTexture(texBinding, texture)
            || !builder.AddSampler(samplerBinding, resources.LinearSampler.Get())) {
            return false;
        }
        return builder.Build(outDesc);
    }

    auto BuildTaaBindGroupDesc(const FPostProcessSharedResources& resources,
        Rhi::FRhiTexture* currentColor, Rhi::FRhiTexture* historyColor,
        Rhi::FRhiTexture* sceneDepth, Rhi::FRhiBindGroupDesc& outDesc) -> bool {
        if (!resources.TaaLayout || !resources.LinearSampler || !resources.TaaConstantsBuffer
            || currentColor == nullptr || historyColor == nullptr || sceneDepth == nullptr) {
            return false;
        }

        auto findBinding = [&](const TChar* name, Rhi::ERhiBindingType type,
                               u32& outBinding) -> bool {
            const auto nameHash =
                RenderCore::ShaderBinding::HashBindingName(Core::Container::FStringView(name));
            return RenderCore::ShaderBinding::FindBindingByNameHash(
                resources.TaaLayoutBindings, nameHash, type, outBinding);
        };

        u32 cbBinding      = RenderCore::ShaderBinding::kInvalidBinding;
        u32 currentBinding = RenderCore::ShaderBinding::kInvalidBinding;
        u32 historyBinding = RenderCore::ShaderBinding::kInvalidBinding;
        u32 depthBinding   = RenderCore::ShaderBinding::kInvalidBinding;
        u32 samplerBinding = RenderCore::ShaderBinding::kInvalidBinding;

        if (!findBinding(kNameTaaConstants, Rhi::ERhiBindingType::ConstantBuffer, cbBinding)
            || !findBinding(kNameCurrentColor, Rhi::ERhiBindingType::SampledTexture, currentBinding)
            || !findBinding(kNameHistoryColor, Rhi::ERhiBindingType::SampledTexture, historyBinding)
            || !findBinding(kNameSceneDepth, Rhi::ERhiBindingType::SampledTexture, depthBinding)
            || !findBinding(kNameLinearSampler, Rhi::ERhiBindingType::Sampler, samplerBinding)) {
            return false;
        }

        RenderCore::ShaderBinding::FBindGroupBuilder builder(resources.TaaLayout.Get());
        if (!builder.AddBuffer(cbBinding, resources.TaaConstantsBuffer.Get(), 0ULL,
                static_cast<u64>(sizeof(FTaaConstants)))
            || !builder.AddTexture(currentBinding, currentColor)
            || !builder.AddTexture(historyBinding, historyColor)
            || !builder.AddTexture(depthBinding, sceneDepth)
            || !builder.AddSampler(samplerBinding, resources.LinearSampler.Get())) {
            return false;
        }
        return builder.Build(outDesc);
    }

    void ShutdownPostProcessSharedResources() noexcept {
        auto& res = GetPostProcessSharedResources();

        res.BlitPipeline.Reset();
        res.TonemapPipeline.Reset();
        res.FxaaPipeline.Reset();
        res.BloomPrefilterPipeline.Reset();
        res.BloomDownsamplePipeline.Reset();
        res.BloomDownsampleWeightedPipeline.Reset();
        res.BloomBlurHPipeline.Reset();
        res.BloomBlurVPipeline.Reset();
        res.BloomUpsampleAddPipeline.Reset();
        res.BloomApplyAddPipeline.Reset();
        res.TaaPipeline.Reset();

        res.BlitConstantsBuffer.Reset();
        res.TonemapConstantsBuffer.Reset();
        res.FxaaConstantsBuffer.Reset();
        res.BloomConstantsBuffer.Reset();
        res.TaaConstantsBuffer.Reset();

        res.LinearSampler.Reset();
        res.PipelineLayout.Reset();
        res.TaaPipelineLayout.Reset();
        res.Layout.Reset();
        res.TaaLayout.Reset();
        res.LayoutBindings.Reset();
        res.TaaLayoutBindings.Reset();

        res.FullscreenVS.Reset();
        res.BlitPS.Reset();
        res.TonemapPS.Reset();
        res.FxaaPS.Reset();
        res.BloomPrefilterPS.Reset();
        res.BloomDownsamplePS.Reset();
        res.BloomDownsampleWeightedPS.Reset();
        res.BloomBlurHPS.Reset();
        res.BloomBlurVPS.Reset();
        res.BloomUpsamplePS.Reset();
        res.BloomApplyPS.Reset();
        res.TaaPS.Reset();
    }
} // namespace AltinaEngine::Rendering::PostProcess::Detail
