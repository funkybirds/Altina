#include "Rendering/CommonRendererResource.h"

#include "Rendering/BasicDeferredRenderer.h"

#include "Algorithm/CStringUtils.h"
#include "Container/SmartPtr.h"
#include "Logging/Log.h"
#include "Material/MaterialPass.h"
#include "Material/MaterialTemplate.h"
#include "Platform/PlatformFileSystem.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiEnums.h"
#include "Rhi/RhiInit.h"
#include "Shader/ShaderReflection.h"
#include "ShaderCompiler/ShaderCompiler.h"
#include "ShaderCompiler/ShaderPermutationParser.h"
#include "ShaderCompiler/ShaderRhiBindings.h"
#include "Shader/ShaderPreset.h"
#include "Types/Traits.h"
#include "Utility/Filesystem/Path.h"
#include "Utility/Filesystem/PathUtils.h"
#include "Utility/String/CodeConvert.h"

namespace AltinaEngine::Rendering {
    namespace {
        namespace Container = Core::Container;
        using AltinaEngine::Move;
        using Container::FString;
        using Container::FStringView;
        using Core::Utility::Filesystem::FPath;
        using RenderCore::EMaterialPass;
        using ShaderCompiler::EShaderOptimization;
        using ShaderCompiler::EShaderSourceLanguage;
        using ShaderCompiler::FShaderCompileRequest;
        using ShaderCompiler::FShaderCompileResult;
        using ShaderCompiler::FShaderPermutationParseResult;
        using ShaderCompiler::GetShaderCompiler;

        // Runtime-staged shader location (preferred): next to the executable under
        // Assets/Shader/... This keeps demo/game builds self-contained without requiring the engine
        // source tree.
        constexpr FStringView kDeferredShaderAssetsRelPath =
            TEXT("Assets/Shader/Deferred/BasicDeferred.hlsl");

        // Legacy runtime-staged shader location (kept for backward compatibility).
        constexpr FStringView kDeferredShaderRelPath = TEXT("Shader/Deferred/BasicDeferred.hlsl");
        constexpr FStringView kDeferredShaderSourcePath =
            TEXT("Source/Shader/Deferred/BasicDeferred.hlsl");

        constexpr FStringView kDeferredLightingShaderAssetsRelPath =
            TEXT("Assets/Shader/Deferred/DeferredLighting.hlsl");
        constexpr FStringView kDeferredLightingShaderRelPath =
            TEXT("Shader/Deferred/DeferredLighting.hlsl");
        constexpr FStringView kDeferredLightingShaderSourcePath =
            TEXT("Source/Shader/Deferred/DeferredLighting.hlsl");

        constexpr FStringView kDeferredSkyBoxShaderAssetsRelPath =
            TEXT("Assets/Shader/Deferred/SkyBox.hlsl");
        constexpr FStringView kDeferredSkyBoxShaderRelPath = TEXT("Shader/Deferred/SkyBox.hlsl");
        constexpr FStringView kDeferredSkyBoxShaderSourcePath =
            TEXT("Source/Shader/Deferred/SkyBox.hlsl");

        constexpr FStringView kShadowDepthShaderAssetsRelPath =
            TEXT("Assets/Shader/Shadow/ShadowDepth.hlsl");
        constexpr FStringView kShadowDepthShaderRelPath = TEXT("Shader/Shadow/ShadowDepth.hlsl");
        constexpr FStringView kShadowDepthShaderSourcePath =
            TEXT("Source/Shader/Shadow/ShadowDepth.hlsl");

        auto FindBuiltinDeferredShaderPath() -> FPath {
            const FPath exeDir(Core::Platform::GetExecutableDir());
            if (!exeDir.IsEmpty()) {
                const auto candidateAssets = exeDir / kDeferredShaderAssetsRelPath;
                if (candidateAssets.Exists()) {
                    return candidateAssets;
                }
                const auto candidate = exeDir / kDeferredShaderRelPath;
                if (candidate.Exists()) {
                    return candidate;
                }

                // Multi-config generators often place the executable under <Binaries>/<Config>/.
                // Demo assets are staged under <Binaries>/Assets, so also probe the parent folder.
                const auto exeParent = exeDir.ParentPath();
                if (!exeParent.IsEmpty() && exeParent != exeDir) {
                    const auto parentAssets = exeParent / kDeferredShaderAssetsRelPath;
                    if (parentAssets.Exists()) {
                        return parentAssets;
                    }
                    const auto parentLegacy = exeParent / kDeferredShaderRelPath;
                    if (parentLegacy.Exists()) {
                        return parentLegacy;
                    }
                }
            }

            const auto cwd = Core::Utility::Filesystem::GetCurrentWorkingDir();
            if (!cwd.IsEmpty()) {
                const auto candidate = cwd / kDeferredShaderSourcePath;
                if (candidate.Exists()) {
                    return candidate;
                }
                const auto candidateAssets = cwd / kDeferredShaderAssetsRelPath;
                if (candidateAssets.Exists()) {
                    return candidateAssets;
                }
                const auto candidate2 = cwd / kDeferredShaderRelPath;
                if (candidate2.Exists()) {
                    return candidate2;
                }
            }

            FPath probe = cwd;
            for (u32 i = 0U; i < 6U && !probe.IsEmpty(); ++i) {
                const auto candidate = probe / kDeferredShaderSourcePath;
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

        auto FindBuiltinShaderPath(FStringView assetsRel, FStringView rel, FStringView sourceRel)
            -> FPath {
            const FPath exeDir(Core::Platform::GetExecutableDir());
            if (!exeDir.IsEmpty()) {
                const auto candidateAssets = exeDir / assetsRel;
                if (candidateAssets.Exists()) {
                    return candidateAssets;
                }
                const auto candidate = exeDir / rel;
                if (candidate.Exists()) {
                    return candidate;
                }

                const auto exeParent = exeDir.ParentPath();
                if (!exeParent.IsEmpty() && exeParent != exeDir) {
                    const auto parentAssets = exeParent / assetsRel;
                    if (parentAssets.Exists()) {
                        return parentAssets;
                    }
                    const auto parentLegacy = exeParent / rel;
                    if (parentLegacy.Exists()) {
                        return parentLegacy;
                    }
                }
            }

            const auto cwd = Core::Utility::Filesystem::GetCurrentWorkingDir();
            if (!cwd.IsEmpty()) {
                const auto candidate = cwd / sourceRel;
                if (candidate.Exists()) {
                    return candidate;
                }
                const auto candidateAssets = cwd / assetsRel;
                if (candidateAssets.Exists()) {
                    return candidateAssets;
                }
                const auto candidate2 = cwd / rel;
                if (candidate2.Exists()) {
                    return candidate2;
                }
            }

            FPath probe = cwd;
            for (u32 i = 0U; i < 6U && !probe.IsEmpty(); ++i) {
                const auto candidate = probe / sourceRel;
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
            auto includeDir = shaderPath.ParentPath().ParentPath().ParentPath();
            if (!includeDir.IsEmpty()) {
                return includeDir;
            }
            return shaderPath.ParentPath();
        }

        auto CompileShaderFromFile(const FPath& path, FStringView entry, Shader::EShaderStage stage,
            const FStringView keyPrefix, RenderCore::FShaderRegistry::FShaderKey& outKey,
            FShaderCompileResult& outResult) -> bool {
            if (path.IsEmpty() || !path.Exists()) {
                LogError(TEXT("Deferred shader source not found: path='{}' entry='{}' stage={}"),
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

            outResult = GetShaderCompiler().Compile(request);
            if (!outResult.mSucceeded) {
                LogError(
                    TEXT("Deferred shader compile failed: path='{}' entry='{}' stage={} diag={}"),
                    path.GetString().ToView(), entry, static_cast<u32>(stage),
                    outResult.mDiagnostics.ToView());
                return false;
            }

            auto* device = Rhi::RHIGetDevice();
            if (!device) {
                LogError(TEXT("RHI device missing for shader creation."));
                return false;
            }

            auto shaderDesc = ShaderCompiler::BuildRhiShaderDesc(outResult);
            shaderDesc.mDebugName.Assign(entry);
            auto shader = device->CreateShader(shaderDesc);
            if (!shader) {
                LogError(
                    TEXT("Failed to create deferred RHI shader: path='{}' entry='{}' stage={}"),
                    path.GetString().ToView(), entry, static_cast<u32>(stage));
                return false;
            }

            FString keyName(keyPrefix);
            keyName.Append(TEXT("."));
            keyName.Append(entry);
            outKey = RenderCore::FShaderRegistry::MakeKey(keyName.ToView(), stage);

            if (!FBasicDeferredRenderer::RegisterShader(outKey, shader)) {
                LogError(
                    TEXT("Failed to register deferred shader: key='{}'"), outKey.Name.ToView());
                return false;
            }

            return true;
        }

        auto IsMaterialCBufferName(FStringView name) -> bool {
            if (name.IsEmpty()) {
                return false;
            }
            const FStringView target(TEXT("MaterialConstants"));
            if (name.Length() < target.Length()) {
                return false;
            }
            for (usize i = 0; i < target.Length(); ++i) {
                if (Core::Algorithm::ToLowerChar(name[i])
                    != Core::Algorithm::ToLowerChar(target[i])) {
                    return false;
                }
            }
            return true;
        }

        auto FindMaterialCBuffer(const Shader::FShaderReflection& reflection)
            -> const Shader::FShaderConstantBuffer* {
            for (const auto& cbuffer : reflection.mConstantBuffers) {
                if (IsMaterialCBufferName(cbuffer.mName.ToView())) {
                    return &cbuffer;
                }
            }
            return nullptr;
        }

        auto SelectMaterialCBuffer(const Shader::FShaderReflection* vertex,
            const Shader::FShaderReflection* pixel) -> const Shader::FShaderConstantBuffer* {
            const Shader::FShaderConstantBuffer* materialCBuffer = nullptr;
            if (pixel != nullptr) {
                materialCBuffer = FindMaterialCBuffer(*pixel);
            }
            if (materialCBuffer == nullptr && vertex != nullptr) {
                materialCBuffer = FindMaterialCBuffer(*vertex);
            }
            return materialCBuffer;
        }

        auto FindSamplerBinding(
            const Shader::FShaderReflection& reflection, const FStringView& textureName) -> u32 {
            for (const auto& resource : reflection.mResources) {
                if (resource.mType != Shader::EShaderResourceType::Sampler) {
                    continue;
                }
                if (resource.mName.ToView() == textureName) {
                    return resource.mBinding;
                }
            }

            FString withSampler(textureName);
            withSampler.Append(TEXT("Sampler"));
            for (const auto& resource : reflection.mResources) {
                if (resource.mType != Shader::EShaderResourceType::Sampler) {
                    continue;
                }
                if (resource.mName.ToView() == withSampler.ToView()) {
                    return resource.mBinding;
                }
            }

            FString withState(textureName);
            withState.Append(TEXT("SamplerState"));
            for (const auto& resource : reflection.mResources) {
                if (resource.mType != Shader::EShaderResourceType::Sampler) {
                    continue;
                }
                if (resource.mName.ToView() == withState.ToView()) {
                    return resource.mBinding;
                }
            }

            return RenderCore::kMaterialInvalidBinding;
        }

        void AddTextureBindings(
            RenderCore::FMaterialLayout& layout, const Shader::FShaderReflection* reflection) {
            if (reflection == nullptr) {
                return;
            }
            for (const auto& resource : reflection->mResources) {
                if (resource.mType != Shader::EShaderResourceType::Texture) {
                    continue;
                }
                const auto nameHash = RenderCore::HashMaterialParamName(resource.mName.ToView());
                if (nameHash == 0U) {
                    continue;
                }
                const u32 samplerBinding = FindSamplerBinding(*reflection, resource.mName.ToView());
                layout.AddTextureBinding(nameHash, resource.mBinding, samplerBinding);
            }
        }

        auto BuildMaterialLayout(const Shader::FShaderReflection* vertex,
            const Shader::FShaderReflection* pixel) -> RenderCore::FMaterialLayout {
            RenderCore::FMaterialLayout layout;
            const auto*                 materialCBuffer = SelectMaterialCBuffer(vertex, pixel);
            if (materialCBuffer == nullptr) {
                return layout;
            }
            layout.InitFromConstantBuffer(*materialCBuffer);
            AddTextureBindings(layout, pixel);
            AddTextureBindings(layout, vertex);
            layout.SortTextureBindings();
            return layout;
        }

        auto TryParseRasterState(const FPath& shaderPath, Shader::FShaderRasterState& out) -> bool {
            Core::Container::FNativeString source;
            if (!Core::Platform::ReadFileTextUtf8(shaderPath.GetString(), source)) {
                return false;
            }
            const auto                    sourceText = Core::Utility::String::FromUtf8(source);
            FShaderPermutationParseResult parse{};
            if (!ShaderCompiler::ParseShaderPermutationSource(sourceText.ToView(), parse)) {
                return false;
            }
            if (!parse.mHasRasterState) {
                return false;
            }
            out = parse.mRasterState;
            return true;
        }
    } // namespace

    void InitCommonRendererResource() {
        static bool sInitialized = false;
        if (sInitialized) {
            return;
        }

        RenderCore::InitCommonShaders();

        const auto shaderPath = FindBuiltinDeferredShaderPath();
        if (shaderPath.IsEmpty() || !shaderPath.Exists()) {
            LogError(
                TEXT("Builtin deferred shader not found. Expected {}."), kDeferredShaderRelPath);
            return;
        }

        const auto lightingShaderPath = FindBuiltinShaderPath(kDeferredLightingShaderAssetsRelPath,
            kDeferredLightingShaderRelPath, kDeferredLightingShaderSourcePath);
        if (lightingShaderPath.IsEmpty() || !lightingShaderPath.Exists()) {
            LogError(TEXT("Builtin deferred lighting shader not found. Expected {}."),
                kDeferredLightingShaderRelPath);
            return;
        }

        const auto skyBoxShaderPath = FindBuiltinShaderPath(kDeferredSkyBoxShaderAssetsRelPath,
            kDeferredSkyBoxShaderRelPath, kDeferredSkyBoxShaderSourcePath);
        if (skyBoxShaderPath.IsEmpty() || !skyBoxShaderPath.Exists()) {
            LogError(TEXT("Builtin deferred skybox shader not found. Expected {}."),
                kDeferredSkyBoxShaderRelPath);
            return;
        }

        const auto shadowShaderPath = FindBuiltinShaderPath(kShadowDepthShaderAssetsRelPath,
            kShadowDepthShaderRelPath, kShadowDepthShaderSourcePath);
        if (shadowShaderPath.IsEmpty() || !shadowShaderPath.Exists()) {
            LogError(TEXT("Builtin shadow depth shader not found. Expected {}."),
                kShadowDepthShaderRelPath);
            return;
        }

        LogInfo(TEXT("Deferred shader paths: base='{}' lighting='{}' skybox='{}' shadow='{}'"),
            shaderPath.GetString().ToView(), lightingShaderPath.GetString().ToView(),
            skyBoxShaderPath.GetString().ToView(), shadowShaderPath.GetString().ToView());

        RenderCore::FShaderRegistry::FShaderKey vsKey{};
        RenderCore::FShaderRegistry::FShaderKey psKey{};
        RenderCore::FShaderRegistry::FShaderKey fsqVsKey{};
        RenderCore::FShaderRegistry::FShaderKey lightingVsKey{};
        RenderCore::FShaderRegistry::FShaderKey lightingPsKey{};
        RenderCore::FShaderRegistry::FShaderKey skyBoxVsKey{};
        RenderCore::FShaderRegistry::FShaderKey skyBoxPsKey{};
        RenderCore::FShaderRegistry::FShaderKey shadowVsKey{};
        RenderCore::FShaderRegistry::FShaderKey shadowPsKey{};
        FShaderCompileResult                    vsResult{};
        FShaderCompileResult                    psResult{};
        FShaderCompileResult                    fsqVsResult{};
        FShaderCompileResult                    lightingVsResult{};
        FShaderCompileResult                    lightingPsResult{};
        FShaderCompileResult                    skyBoxVsResult{};
        FShaderCompileResult                    skyBoxPsResult{};
        FShaderCompileResult                    shadowVsResult{};
        FShaderCompileResult                    shadowPsResult{};

        constexpr FStringView                   kKeyPrefix = TEXT("Builtin/Deferred/BasicDeferred");
        if (!CompileShaderFromFile(shaderPath, TEXT("VSBase"), Shader::EShaderStage::Vertex,
                kKeyPrefix, vsKey, vsResult)
            || !CompileShaderFromFile(shaderPath, TEXT("PSBase"), Shader::EShaderStage::Pixel,
                kKeyPrefix, psKey, psResult)
            || !CompileShaderFromFile(shaderPath, TEXT("VSComposite"), Shader::EShaderStage::Vertex,
                kKeyPrefix, fsqVsKey, fsqVsResult)
            || !CompileShaderFromFile(lightingShaderPath, TEXT("VSFullScreenTriangle"),
                Shader::EShaderStage::Vertex, TEXT("Builtin/Deferred/DeferredLighting"),
                lightingVsKey, lightingVsResult)
            || !CompileShaderFromFile(lightingShaderPath, TEXT("PSDeferredLighting"),
                Shader::EShaderStage::Pixel, TEXT("Builtin/Deferred/DeferredLighting"),
                lightingPsKey, lightingPsResult)
            || !CompileShaderFromFile(skyBoxShaderPath, TEXT("VSFullScreenTriangle"),
                Shader::EShaderStage::Vertex, TEXT("Builtin/Deferred/SkyBox"), skyBoxVsKey,
                skyBoxVsResult)
            || !CompileShaderFromFile(skyBoxShaderPath, TEXT("PSSkyBox"),
                Shader::EShaderStage::Pixel, TEXT("Builtin/Deferred/SkyBox"), skyBoxPsKey,
                skyBoxPsResult)
            || !CompileShaderFromFile(shadowShaderPath, TEXT("VSShadowDepth"),
                Shader::EShaderStage::Vertex, TEXT("Builtin/Shadow/ShadowDepth"), shadowVsKey,
                shadowVsResult)
            || !CompileShaderFromFile(shadowShaderPath, TEXT("PSShadowDepth"),
                Shader::EShaderStage::Pixel, TEXT("Builtin/Shadow/ShadowDepth"), shadowPsKey,
                shadowPsResult)) {
            return;
        }

        auto templ = Container::MakeShared<RenderCore::FMaterialTemplate>();
        RenderCore::FMaterialPassDesc passDesc{};
        passDesc.Shaders.Vertex = vsKey;
        passDesc.Shaders.Pixel  = psKey;
        passDesc.Layout         = BuildMaterialLayout(&vsResult.mReflection, &psResult.mReflection);
        passDesc.State.Depth.mDepthEnable  = true;
        passDesc.State.Depth.mDepthWrite   = true;
        passDesc.State.Depth.mDepthCompare = Rhi::ERhiCompareOp::GreaterEqual;

        Shader::FShaderRasterState rasterState{};
        if (TryParseRasterState(shaderPath, rasterState)) {
            passDesc.State.ApplyRasterState(rasterState);
        }

        // Renderer default: cull front (can still be overridden by material raster overrides
        // later).
        passDesc.State.Raster.mCullMode = Rhi::ERhiRasterCullMode::Front;

        templ->SetPassDesc(EMaterialPass::BasePass, Move(passDesc));

        // Shadow depth-only pass (Directional CSM).
        RenderCore::FMaterialPassDesc shadowPass{};
        shadowPass.Shaders.Vertex            = shadowVsKey;
        shadowPass.Shaders.Pixel             = shadowPsKey;
        shadowPass.State.Depth.mDepthEnable  = true;
        shadowPass.State.Depth.mDepthWrite   = true;
        shadowPass.State.Depth.mDepthCompare = Rhi::ERhiCompareOp::GreaterEqual;
        // NOTE:
        // Reverse-Z + D32F depth makes the traditional large positive depth-bias values extremely
        // aggressive and can effectively flatten the shadow map to the clear value in some
        // scenes/drivers. Start bias-free for correctness; we can re-introduce tuned bias later.
        shadowPass.State.Raster.mCullMode             = Rhi::ERhiRasterCullMode::None;
        shadowPass.State.Raster.mDepthBias            = 0;
        shadowPass.State.Raster.mSlopeScaledDepthBias = 0.0f;
        templ->SetPassDesc(EMaterialPass::ShadowPass, Move(shadowPass));

        FBasicDeferredRenderer::SetDefaultMaterialTemplate(templ);
        FBasicDeferredRenderer::SetLightingShaderKeys(lightingVsKey, lightingPsKey);
        FBasicDeferredRenderer::SetSkyBoxShaderKeys(skyBoxVsKey, skyBoxPsKey);
        LogInfo(TEXT("Deferred lighting shader keys configured: vs='{}' ps='{}'"),
            lightingVsKey.Name.ToView(), lightingPsKey.Name.ToView());
        sInitialized = true;
    }
} // namespace AltinaEngine::Rendering
