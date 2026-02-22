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

        constexpr FStringView kDeferredShaderRelPath = TEXT("Shader/Deferred/BasicDeferred.hlsl");
        constexpr FStringView kDeferredShaderSourcePath =
            TEXT("Source/Shader/Deferred/BasicDeferred.hlsl");

        auto FindBuiltinDeferredShaderPath() -> FPath {
            const FPath exeDir(Core::Platform::GetExecutableDir());
            if (!exeDir.IsEmpty()) {
                const auto candidate = exeDir / kDeferredShaderRelPath;
                if (candidate.Exists()) {
                    return candidate;
                }
            }

            const auto cwd = Core::Utility::Filesystem::GetCurrentWorkingDir();
            if (!cwd.IsEmpty()) {
                const auto candidate = cwd / kDeferredShaderSourcePath;
                if (candidate.Exists()) {
                    return candidate;
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
                LogError(TEXT("Deferred shader source not found."));
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
                    TEXT("Deferred shader compile failed: {}"), outResult.mDiagnostics.ToView());
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
                LogError(TEXT("Failed to create deferred RHI shader."));
                return false;
            }

            FString keyName(keyPrefix);
            keyName.Append(TEXT("."));
            keyName.Append(entry);
            outKey = RenderCore::FShaderRegistry::MakeKey(keyName.ToView(), stage);

            if (!FBasicDeferredRenderer::RegisterShader(outKey, shader)) {
                LogError(TEXT("Failed to register deferred shader {}."), outKey.Name.ToView());
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

        if (FBasicDeferredRenderer::GetDefaultMaterialTemplate()) {
            sInitialized = true;
            return;
        }

        const auto shaderPath = FindBuiltinDeferredShaderPath();
        if (shaderPath.IsEmpty() || !shaderPath.Exists()) {
            LogError(
                TEXT("Builtin deferred shader not found. Expected {}."), kDeferredShaderRelPath);
            return;
        }

        RenderCore::FShaderRegistry::FShaderKey vsKey{};
        RenderCore::FShaderRegistry::FShaderKey psKey{};
        RenderCore::FShaderRegistry::FShaderKey outVsKey{};
        RenderCore::FShaderRegistry::FShaderKey outPsKey{};
        FShaderCompileResult                    vsResult{};
        FShaderCompileResult                    psResult{};
        FShaderCompileResult                    outVsResult{};
        FShaderCompileResult                    outPsResult{};

        constexpr FStringView                   kKeyPrefix = TEXT("Builtin/Deferred/BasicDeferred");
        if (!CompileShaderFromFile(shaderPath, TEXT("VSBase"), Shader::EShaderStage::Vertex,
                kKeyPrefix, vsKey, vsResult)
            || !CompileShaderFromFile(shaderPath, TEXT("PSBase"), Shader::EShaderStage::Pixel,
                kKeyPrefix, psKey, psResult)
            || !CompileShaderFromFile(shaderPath, TEXT("VSComposite"), Shader::EShaderStage::Vertex,
                kKeyPrefix, outVsKey, outVsResult)
            || !CompileShaderFromFile(shaderPath, TEXT("PSComposite"), Shader::EShaderStage::Pixel,
                kKeyPrefix, outPsKey, outPsResult)) {
            return;
        }

        auto templ = Container::MakeShared<RenderCore::FMaterialTemplate>();
        RenderCore::FMaterialPassDesc passDesc{};
        passDesc.Shaders.Vertex = vsKey;
        passDesc.Shaders.Pixel  = psKey;
        passDesc.Layout         = BuildMaterialLayout(&vsResult.mReflection, &psResult.mReflection);
        passDesc.State.Depth.mDepthEnable  = true;
        passDesc.State.Depth.mDepthWrite   = true;
        passDesc.State.Depth.mDepthCompare = Rhi::ERhiCompareOp::LessEqual;

        Shader::FShaderRasterState rasterState{};
        if (TryParseRasterState(shaderPath, rasterState)) {
            passDesc.State.ApplyRasterState(rasterState);
        }

        templ->SetPassDesc(EMaterialPass::BasePass, Move(passDesc));
        FBasicDeferredRenderer::SetDefaultMaterialTemplate(templ);
        FBasicDeferredRenderer::SetOutputShaderKeys(outVsKey, outPsKey);
        sInitialized = true;
    }
} // namespace AltinaEngine::Rendering
