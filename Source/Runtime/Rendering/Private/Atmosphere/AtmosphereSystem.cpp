#include "Atmosphere/AtmosphereSystem.h"

#include "Container/SmartPtr.h"
#include "Logging/Log.h"
#include "Math/LinAlg/LookAt.h"
#include "Math/Vector.h"
#include "Platform/PlatformFileSystem.h"
#include "Rhi/Command/RhiCmdContextAdapter.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiCommandContext.h"
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

namespace AltinaEngine::Rendering::Atmosphere {
    namespace {
        namespace Container = Core::Container;
        using Core::Container::MakeUnique;
        using Core::Math::FVector3f;
        using Core::Math::FVector4f;
        using Core::Utility::Filesystem::FPath;
        using RenderCore::ShaderBinding::BuildBindGroupLayoutFromShaders;
        using RenderCore::ShaderBinding::BuildBindingLookupTableFromShaders;
        using RenderCore::ShaderBinding::FBindGroupBuilder;
        using RenderCore::ShaderBinding::FBindingLookupTable;
        using RenderCore::ShaderBinding::FindBindingByNameHash;
        using RenderCore::ShaderBinding::HashBindingName;
        using Rhi::ERhiBindingType;
        using Rhi::ERhiFormat;
        using Rhi::ERhiTextureBindFlags;
        using Rhi::ERhiTextureDimension;
        using Shader::EShaderStage;
        using ShaderCompiler::BuildRhiShaderDesc;
        using ShaderCompiler::EShaderOptimization;
        using ShaderCompiler::EShaderSourceLanguage;
        using ShaderCompiler::FShaderCompileRequest;
        using ShaderCompiler::FShaderCompileResult;
        using ShaderCompiler::GetShaderCompiler;

        constexpr u32                    kTransmittanceWidth  = 256U;
        constexpr u32                    kTransmittanceHeight = 64U;
        constexpr u32                    kIrradianceWidth     = 64U;
        constexpr u32                    kIrradianceHeight    = 16U;
        constexpr u32                    kScatteringWidth     = 32U * 32U;
        constexpr u32                    kScatteringHeight    = 128U;
        constexpr u32                    kScatteringDepth     = 32U;
        constexpr u32                    kThreadGroupSizeX    = 16U;
        constexpr u32                    kThreadGroupSizeY    = 16U;

        constexpr Container::FStringView kAtmosphereShaderAssetsRelDir =
            TEXT("Assets/Shader/Atmosphere");
        constexpr Container::FStringView kAtmosphereShaderRelDir = TEXT("Shader/Atmosphere");
        constexpr Container::FStringView kAtmosphereShaderSourceRelDir =
            TEXT("Source/Shader/Atmosphere");

        constexpr Container::FStringView kTransmittanceShaderFile =
            TEXT("AtmospherePrecompute.Transmittance.hlsl");
        constexpr Container::FStringView kIrradianceShaderFile =
            TEXT("AtmospherePrecompute.Irradiance.hlsl");
        constexpr Container::FStringView kSingleScatteringShaderFile =
            TEXT("AtmospherePrecompute.SingleScattering.hlsl");
        constexpr Container::FStringView kScatteringDensityShaderFile =
            TEXT("AtmospherePrecompute.ScatteringDensity.hlsl");
        constexpr Container::FStringView kIndirectIrradianceShaderFile =
            TEXT("AtmospherePrecompute.IndirectIrradiance.hlsl");
        constexpr Container::FStringView kMultipleScatteringShaderFile =
            TEXT("AtmospherePrecompute.MultipleScattering.hlsl");
        constexpr u32 kMaxScatteringOrder = 4U;

        struct FAtmosphereComputeConstants {
            FVector4f mRayleighScattering_RayleighScaleHeightKm = FVector4f(0.0f);
            FVector4f mMieScattering_MieScaleHeightKm           = FVector4f(0.0f);
            FVector4f mMieAbsorption_MieAnisotropy              = FVector4f(0.0f);
            FVector4f mOzoneAbsorption_OzoneCenterHeightKm      = FVector4f(0.0f);
            FVector4f mGroundAlbedo_OzoneThicknessKm            = FVector4f(0.0f);
            FVector4f mSolarTint_SolarIlluminance               = FVector4f(0.0f);
            FVector4f mSunDirection_Roughness                   = FVector4f(0.0f, 1.0f, 0.0f, 0.0f);
            FVector4f mGeometry                                 = FVector4f(0.0f);
            FVector4f mOutput                                   = FVector4f(0.0f);
        };

        struct FAtmosphereComputePassResources {
            Rhi::FRhiShaderRef          mShader;
            Rhi::FRhiBindGroupLayoutRef mLayout;
            FBindingLookupTable         mBindings;
            Rhi::FRhiPipelineLayoutRef  mPipelineLayout;
            Rhi::FRhiPipelineRef        mPipeline;

            [[nodiscard]] auto          IsValid() const noexcept -> bool {
                return mShader && mLayout && mPipelineLayout && mPipeline;
            }
        };

        [[nodiscard]] auto ResolveShaderTargetBackend() noexcept -> Rhi::ERhiBackend {
            const auto backend = Rhi::RHIGetBackend();
            return (backend != Rhi::ERhiBackend::Unknown) ? backend : Rhi::ERhiBackend::DirectX11;
        }

        [[nodiscard]] auto ClampDir(const FVector3f& direction) noexcept -> FVector3f {
            const f32 lenSq = Core::Math::LinAlg::Dot(direction, direction);
            if (lenSq <= 1e-6f) {
                return FVector3f(0.0f, 1.0f, 0.0f);
            }
            return Core::Math::LinAlg::Normalize(direction);
        }

        [[nodiscard]] auto IsSameVector(const FVector3f& lhs, const FVector3f& rhs) noexcept
            -> bool {
            return lhs.X() == rhs.X() && lhs.Y() == rhs.Y() && lhs.Z() == rhs.Z();
        }

        [[nodiscard]] auto DivideRoundUp(u32 value, u32 divisor) noexcept -> u32 {
            return (value + divisor - 1U) / divisor;
        }

        [[nodiscard]] auto FindBuiltinShaderPath(Container::FStringView fileName) -> FPath {
            const FPath exeDir(Core::Platform::GetExecutableDir());
            if (!exeDir.IsEmpty()) {
                const auto assetPath = exeDir / kAtmosphereShaderAssetsRelDir / fileName;
                if (assetPath.Exists()) {
                    return assetPath;
                }
                const auto legacyPath = exeDir / kAtmosphereShaderRelDir / fileName;
                if (legacyPath.Exists()) {
                    return legacyPath;
                }
            }

            const auto cwd = Core::Utility::Filesystem::GetCurrentWorkingDir();
            if (!cwd.IsEmpty()) {
                const auto sourcePath = cwd / kAtmosphereShaderSourceRelDir / fileName;
                if (sourcePath.Exists()) {
                    return sourcePath;
                }
                const auto assetPath = cwd / kAtmosphereShaderAssetsRelDir / fileName;
                if (assetPath.Exists()) {
                    return assetPath;
                }
            }

            FPath probe = cwd;
            for (u32 i = 0U; i < 6U && !probe.IsEmpty(); ++i) {
                const auto sourcePath = probe / kAtmosphereShaderSourceRelDir / fileName;
                if (sourcePath.Exists()) {
                    return sourcePath;
                }
                const auto parent = probe.ParentPath();
                if (parent == probe) {
                    break;
                }
                probe = parent;
            }

            return {};
        }

        [[nodiscard]] auto BuildIncludeDir(const FPath& shaderPath) -> FPath {
            auto includeDir = shaderPath.ParentPath().ParentPath().ParentPath();
            if (!includeDir.IsEmpty()) {
                return includeDir;
            }
            return shaderPath.ParentPath();
        }

        auto CompileShaderFromFile(Rhi::FRhiDevice& device, const FPath& path,
            Container::FStringView entry, EShaderStage stage, Rhi::FRhiShaderRef& outShader)
            -> bool {
            if (path.IsEmpty() || !path.Exists()) {
                LogError(
                    TEXT("Atmosphere shader source not found: '{}'"), path.GetString().ToView());
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

            request.mOptions.mTargetBackend = ResolveShaderTargetBackend();
            request.mOptions.mOptimization  = EShaderOptimization::Default;
            request.mOptions.mDebugInfo     = false;

            const FShaderCompileResult result = GetShaderCompiler().Compile(request);
            if (!result.mSucceeded) {
                LogError(TEXT("Atmosphere shader compile failed: path='{}' entry='{}' diag={}"),
                    path.GetString().ToView(), entry, result.mDiagnostics.ToView());
                return false;
            }

            auto shaderDesc = BuildRhiShaderDesc(result);
            shaderDesc.mDebugName.Assign(entry);
            outShader = device.CreateShader(shaderDesc);
            return outShader.Get() != nullptr;
        }

        auto BuildComputePass(Rhi::FRhiDevice& device, const Rhi::FRhiShaderRef& shader,
            const TChar* debugName, FAtmosphereComputePassResources& outPass) -> bool {
            if (!shader) {
                return false;
            }

            outPass         = {};
            outPass.mShader = shader;

            Container::TVector<Rhi::FRhiShader*> shaders{};
            shaders.PushBack(shader.Get());

            Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
            if (!BuildBindGroupLayoutFromShaders(shaders, 0U, layoutDesc)) {
                LogError(TEXT("Atmosphere failed to build bind group layout: {}"), debugName);
                return false;
            }
            layoutDesc.mDebugName.Assign(debugName);
            layoutDesc.mDebugName.Append(TEXT(".Layout"));
            outPass.mLayout = device.CreateBindGroupLayout(layoutDesc);
            if (!outPass.mLayout) {
                return false;
            }

            if (!BuildBindingLookupTableFromShaders(
                    shaders, 0U, outPass.mLayout.Get(), outPass.mBindings)) {
                return false;
            }

            Rhi::FRhiPipelineLayoutDesc pipelineLayoutDesc{};
            pipelineLayoutDesc.mDebugName.Assign(debugName);
            pipelineLayoutDesc.mDebugName.Append(TEXT(".PipelineLayout"));
            pipelineLayoutDesc.mBindGroupLayouts.PushBack(outPass.mLayout.Get());
            outPass.mPipelineLayout = device.CreatePipelineLayout(pipelineLayoutDesc);
            if (!outPass.mPipelineLayout) {
                return false;
            }

            Rhi::FRhiComputePipelineDesc pipelineDesc{};
            pipelineDesc.mDebugName.Assign(debugName);
            pipelineDesc.mComputeShader  = outPass.mShader.Get();
            pipelineDesc.mPipelineLayout = outPass.mPipelineLayout.Get();
            outPass.mPipeline            = device.CreateComputePipeline(pipelineDesc);
            return outPass.mPipeline.Get() != nullptr;
        }

        [[nodiscard]] auto FindRequiredBinding(const FBindingLookupTable& table, const TChar* name,
            ERhiBindingType type, u32& outBinding) -> bool {
            return FindBindingByNameHash(table, HashBindingName(name), type, outBinding);
        }

        auto CreateTexture(const TChar* name, ERhiTextureDimension dimension, u32 width, u32 height,
            u32 depth) -> Rhi::FRhiTextureRef {
            Rhi::FRhiTextureDesc desc{};
            desc.mDebugName.Assign(name);
            desc.mDimension   = dimension;
            desc.mWidth       = width;
            desc.mHeight      = height;
            desc.mDepth       = depth;
            desc.mMipLevels   = 1U;
            desc.mArrayLayers = 1U;
            desc.mFormat      = ERhiFormat::R16G16B16A16Float;
            desc.mBindFlags =
                ERhiTextureBindFlags::ShaderResource | ERhiTextureBindFlags::UnorderedAccess;
            return Rhi::RHICreateTexture(desc);
        }

        [[nodiscard]] auto BuildSrv(Rhi::FRhiDevice& device, Rhi::FRhiTexture* texture,
            ERhiFormat format, u32 mipLevels, u32 arrayLayers, u32 depth, const TChar* name)
            -> Rhi::FRhiShaderResourceViewRef {
            if (texture == nullptr) {
                return {};
            }
            Rhi::FRhiShaderResourceViewDesc srvDesc{};
            srvDesc.mDebugName.Assign(name);
            srvDesc.mTexture                       = texture;
            srvDesc.mFormat                        = format;
            srvDesc.mTextureRange.mBaseMip         = 0U;
            srvDesc.mTextureRange.mMipCount        = mipLevels;
            srvDesc.mTextureRange.mBaseArrayLayer  = 0U;
            srvDesc.mTextureRange.mLayerCount      = arrayLayers;
            srvDesc.mTextureRange.mBaseDepthSlice  = 0U;
            srvDesc.mTextureRange.mDepthSliceCount = depth;
            return device.CreateShaderResourceView(srvDesc);
        }

        auto CreateAtmosphereConstantBuffer(Rhi::FRhiDevice& device) -> Rhi::FRhiBufferRef {
            Rhi::FRhiBufferDesc desc{};
            desc.mDebugName.Assign(TEXT("Atmosphere.Params"));
            desc.mSizeBytes = sizeof(FAtmosphereComputeConstants);
            desc.mUsage     = Rhi::ERhiResourceUsage::Dynamic;
            desc.mBindFlags = Rhi::ERhiBufferBindFlags::Constant;
            desc.mCpuAccess = Rhi::ERhiCpuAccess::Write;
            return device.CreateBuffer(desc);
        }

        auto BuildConstants(const FAtmosphereSkyDesc& desc, const FVector3f& sunDirection)
            -> FAtmosphereComputeConstants {
            FAtmosphereComputeConstants constants{};
            constants.mRayleighScattering_RayleighScaleHeightKm =
                FVector4f(desc.mRayleighScattering.X(), desc.mRayleighScattering.Y(),
                    desc.mRayleighScattering.Z(), desc.mRayleighScaleHeightKm);
            constants.mMieScattering_MieScaleHeightKm      = FVector4f(desc.mMieScattering.X(),
                     desc.mMieScattering.Y(), desc.mMieScattering.Z(), desc.mMieScaleHeightKm);
            constants.mMieAbsorption_MieAnisotropy         = FVector4f(desc.mMieAbsorption.X(),
                        desc.mMieAbsorption.Y(), desc.mMieAbsorption.Z(), desc.mMieAnisotropy);
            constants.mOzoneAbsorption_OzoneCenterHeightKm = FVector4f(desc.mOzoneAbsorption.X(),
                desc.mOzoneAbsorption.Y(), desc.mOzoneAbsorption.Z(), desc.mOzoneCenterHeightKm);
            constants.mGroundAlbedo_OzoneThicknessKm       = FVector4f(desc.mGroundAlbedo.X(),
                      desc.mGroundAlbedo.Y(), desc.mGroundAlbedo.Z(), desc.mOzoneThicknessKm);
            constants.mSolarTint_SolarIlluminance          = FVector4f(desc.mSolarTint.X(),
                         desc.mSolarTint.Y(), desc.mSolarTint.Z(), desc.mSolarIlluminance);
            constants.mSunDirection_Roughness =
                FVector4f(-sunDirection.X(), -sunDirection.Y(), -sunDirection.Z(), 0.0f);
            constants.mGeometry = FVector4f(desc.mSunAngularRadius, desc.mPlanetRadiusKm,
                desc.mAtmosphereHeightKm, desc.mViewHeightKm);
            constants.mOutput   = FVector4f(desc.mExposure, 0.0f, 0.0f, 0.0f);
            return constants;
        }

        auto BuildTransmittanceBindGroup(Rhi::FRhiDevice& device,
            const FAtmosphereComputePassResources& pass, Rhi::FRhiBuffer* constantBuffer,
            Rhi::FRhiTexture* outputTexture, Rhi::FRhiBindGroupRef& outGroup) -> bool {
            u32 cbBinding  = 0U;
            u32 outBinding = 0U;
            if (!FindRequiredBinding(pass.mBindings, TEXT("AtmosphereParams"),
                    ERhiBindingType::ConstantBuffer, cbBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("OutTransmittance"),
                    ERhiBindingType::StorageTexture, outBinding)) {
                return false;
            }

            FBindGroupBuilder builder(pass.mLayout.Get());
            if (!builder.AddBuffer(
                    cbBinding, constantBuffer, 0ULL, sizeof(FAtmosphereComputeConstants))
                || !builder.AddStorageTexture(outBinding, outputTexture)) {
                return false;
            }

            Rhi::FRhiBindGroupDesc groupDesc{};
            groupDesc.mDebugName.Assign(TEXT("Atmosphere.Transmittance.BindGroup"));
            if (!builder.Build(groupDesc)) {
                return false;
            }
            outGroup = device.CreateBindGroup(groupDesc);
            return outGroup.Get() != nullptr;
        }

        auto BuildIrradianceBindGroup(Rhi::FRhiDevice& device,
            const FAtmosphereComputePassResources& pass, Rhi::FRhiBuffer* constantBuffer,
            Rhi::FRhiTexture* transmittanceTexture, Rhi::FRhiSampler* sampler,
            Rhi::FRhiTexture* outputTexture, Rhi::FRhiBindGroupRef& outGroup) -> bool {
            u32 cbBinding      = 0U;
            u32 transBinding   = 0U;
            u32 samplerBinding = 0U;
            u32 outBinding     = 0U;
            if (!FindRequiredBinding(pass.mBindings, TEXT("AtmosphereParams"),
                    ERhiBindingType::ConstantBuffer, cbBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("TransmittanceLut"),
                    ERhiBindingType::SampledTexture, transBinding)
                || !FindRequiredBinding(
                    pass.mBindings, TEXT("LinearSampler"), ERhiBindingType::Sampler, samplerBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("OutIrradiance"),
                    ERhiBindingType::StorageTexture, outBinding)) {
                return false;
            }

            FBindGroupBuilder builder(pass.mLayout.Get());
            if (!builder.AddBuffer(
                    cbBinding, constantBuffer, 0ULL, sizeof(FAtmosphereComputeConstants))
                || !builder.AddTexture(transBinding, transmittanceTexture)
                || !builder.AddSampler(samplerBinding, sampler)
                || !builder.AddStorageTexture(outBinding, outputTexture)) {
                return false;
            }

            Rhi::FRhiBindGroupDesc groupDesc{};
            groupDesc.mDebugName.Assign(TEXT("Atmosphere.Irradiance.BindGroup"));
            if (!builder.Build(groupDesc)) {
                return false;
            }
            outGroup = device.CreateBindGroup(groupDesc);
            return outGroup.Get() != nullptr;
        }

        auto BuildSingleScatteringBindGroup(Rhi::FRhiDevice& device,
            const FAtmosphereComputePassResources& pass, Rhi::FRhiBuffer* constantBuffer,
            Rhi::FRhiTexture* transmittanceTexture, Rhi::FRhiSampler* sampler,
            Rhi::FRhiTexture* singleRayleighTexture, Rhi::FRhiTexture* scatteringTexture,
            Rhi::FRhiTexture* singleMieTexture, Rhi::FRhiBindGroupRef& outGroup) -> bool {
            u32 cbBinding             = 0U;
            u32 transBinding          = 0U;
            u32 samplerBinding        = 0U;
            u32 singleRayleighBinding = 0U;
            u32 scatteringBinding     = 0U;
            u32 singleMieBinding      = 0U;
            if (!FindRequiredBinding(pass.mBindings, TEXT("AtmosphereParams"),
                    ERhiBindingType::ConstantBuffer, cbBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("TransmittanceLut"),
                    ERhiBindingType::SampledTexture, transBinding)
                || !FindRequiredBinding(
                    pass.mBindings, TEXT("LinearSampler"), ERhiBindingType::Sampler, samplerBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("OutSingleRayleighScattering"),
                    ERhiBindingType::StorageTexture, singleRayleighBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("OutScattering"),
                    ERhiBindingType::StorageTexture, scatteringBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("OutSingleMieScattering"),
                    ERhiBindingType::StorageTexture, singleMieBinding)) {
                return false;
            }

            FBindGroupBuilder builder(pass.mLayout.Get());
            if (!builder.AddBuffer(
                    cbBinding, constantBuffer, 0ULL, sizeof(FAtmosphereComputeConstants))
                || !builder.AddTexture(transBinding, transmittanceTexture)
                || !builder.AddSampler(samplerBinding, sampler)
                || !builder.AddStorageTexture(singleRayleighBinding, singleRayleighTexture)
                || !builder.AddStorageTexture(scatteringBinding, scatteringTexture)
                || !builder.AddStorageTexture(singleMieBinding, singleMieTexture)) {
                return false;
            }

            Rhi::FRhiBindGroupDesc groupDesc{};
            groupDesc.mDebugName.Assign(TEXT("Atmosphere.SingleScattering.BindGroup"));
            if (!builder.Build(groupDesc)) {
                return false;
            }
            outGroup = device.CreateBindGroup(groupDesc);
            return outGroup.Get() != nullptr;
        }

        auto BuildScatteringDensityBindGroup(Rhi::FRhiDevice& device,
            const FAtmosphereComputePassResources& pass, Rhi::FRhiBuffer* constantBuffer,
            Rhi::FRhiTexture* transmittanceTexture, Rhi::FRhiTexture* singleRayleighTexture,
            Rhi::FRhiTexture* singleMieTexture, Rhi::FRhiTexture* multipleScatteringTexture,
            Rhi::FRhiTexture* irradianceTexture, Rhi::FRhiSampler* sampler,
            Rhi::FRhiTexture* outputTexture, Rhi::FRhiBindGroupRef& outGroup) -> bool {
            u32 cbBinding                 = 0U;
            u32 transBinding              = 0U;
            u32 singleRayleighBinding     = 0U;
            u32 singleMieBinding          = 0U;
            u32 multipleScatteringBinding = 0U;
            u32 irradianceBinding         = 0U;
            u32 samplerBinding            = 0U;
            u32 outputBinding             = 0U;
            if (!FindRequiredBinding(pass.mBindings, TEXT("AtmosphereParams"),
                    ERhiBindingType::ConstantBuffer, cbBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("TransmittanceLut"),
                    ERhiBindingType::SampledTexture, transBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("SingleRayleighScatteringLut"),
                    ERhiBindingType::SampledTexture, singleRayleighBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("SingleMieScatteringLut"),
                    ERhiBindingType::SampledTexture, singleMieBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("MultipleScatteringLut"),
                    ERhiBindingType::SampledTexture, multipleScatteringBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("IrradianceLut"),
                    ERhiBindingType::SampledTexture, irradianceBinding)
                || !FindRequiredBinding(
                    pass.mBindings, TEXT("LinearSampler"), ERhiBindingType::Sampler, samplerBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("OutScatteringDensity"),
                    ERhiBindingType::StorageTexture, outputBinding)) {
                return false;
            }

            FBindGroupBuilder builder(pass.mLayout.Get());
            if (!builder.AddBuffer(
                    cbBinding, constantBuffer, 0ULL, sizeof(FAtmosphereComputeConstants))
                || !builder.AddTexture(transBinding, transmittanceTexture)
                || !builder.AddTexture(singleRayleighBinding, singleRayleighTexture)
                || !builder.AddTexture(singleMieBinding, singleMieTexture)
                || !builder.AddTexture(multipleScatteringBinding, multipleScatteringTexture)
                || !builder.AddTexture(irradianceBinding, irradianceTexture)
                || !builder.AddSampler(samplerBinding, sampler)
                || !builder.AddStorageTexture(outputBinding, outputTexture)) {
                return false;
            }

            Rhi::FRhiBindGroupDesc groupDesc{};
            groupDesc.mDebugName.Assign(TEXT("Atmosphere.ScatteringDensity.BindGroup"));
            if (!builder.Build(groupDesc)) {
                return false;
            }
            outGroup = device.CreateBindGroup(groupDesc);
            return outGroup.Get() != nullptr;
        }

        auto BuildIndirectIrradianceBindGroup(Rhi::FRhiDevice& device,
            const FAtmosphereComputePassResources& pass, Rhi::FRhiBuffer* constantBuffer,
            Rhi::FRhiTexture* singleRayleighTexture, Rhi::FRhiTexture* singleMieTexture,
            Rhi::FRhiTexture* multipleScatteringTexture, Rhi::FRhiTexture* irradianceTexture,
            Rhi::FRhiSampler* sampler, Rhi::FRhiTexture* deltaIrradianceTexture,
            Rhi::FRhiBindGroupRef& outGroup) -> bool {
            u32 cbBinding                 = 0U;
            u32 singleRayleighBinding     = 0U;
            u32 singleMieBinding          = 0U;
            u32 multipleScatteringBinding = 0U;
            u32 irradianceBinding         = 0U;
            u32 samplerBinding            = 0U;
            u32 deltaBinding              = 0U;
            u32 outBinding                = 0U;
            if (!FindRequiredBinding(pass.mBindings, TEXT("AtmosphereParams"),
                    ERhiBindingType::ConstantBuffer, cbBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("SingleRayleighScatteringLut"),
                    ERhiBindingType::SampledTexture, singleRayleighBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("SingleMieScatteringLut"),
                    ERhiBindingType::SampledTexture, singleMieBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("MultipleScatteringLut"),
                    ERhiBindingType::SampledTexture, multipleScatteringBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("IrradianceLut"),
                    ERhiBindingType::SampledTexture, irradianceBinding)
                || !FindRequiredBinding(
                    pass.mBindings, TEXT("LinearSampler"), ERhiBindingType::Sampler, samplerBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("OutDeltaIrradiance"),
                    ERhiBindingType::StorageTexture, deltaBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("OutIrradiance"),
                    ERhiBindingType::StorageTexture, outBinding)) {
                return false;
            }

            FBindGroupBuilder builder(pass.mLayout.Get());
            if (!builder.AddBuffer(
                    cbBinding, constantBuffer, 0ULL, sizeof(FAtmosphereComputeConstants))
                || !builder.AddTexture(singleRayleighBinding, singleRayleighTexture)
                || !builder.AddTexture(singleMieBinding, singleMieTexture)
                || !builder.AddTexture(multipleScatteringBinding, multipleScatteringTexture)
                || !builder.AddTexture(irradianceBinding, irradianceTexture)
                || !builder.AddSampler(samplerBinding, sampler)
                || !builder.AddStorageTexture(deltaBinding, deltaIrradianceTexture)
                || !builder.AddStorageTexture(outBinding, irradianceTexture)) {
                return false;
            }

            Rhi::FRhiBindGroupDesc groupDesc{};
            groupDesc.mDebugName.Assign(TEXT("Atmosphere.IndirectIrradiance.BindGroup"));
            if (!builder.Build(groupDesc)) {
                return false;
            }
            outGroup = device.CreateBindGroup(groupDesc);
            return outGroup.Get() != nullptr;
        }

        auto BuildMultipleScatteringBindGroup(Rhi::FRhiDevice& device,
            const FAtmosphereComputePassResources& pass, Rhi::FRhiBuffer* constantBuffer,
            Rhi::FRhiTexture* transmittanceTexture, Rhi::FRhiTexture* scatteringDensityTexture,
            Rhi::FRhiSampler* sampler, Rhi::FRhiTexture* deltaMultipleScatteringTexture,
            Rhi::FRhiTexture* scatteringTexture, Rhi::FRhiBindGroupRef& outGroup) -> bool {
            u32 cbBinding                = 0U;
            u32 transBinding             = 0U;
            u32 scatteringDensityBinding = 0U;
            u32 samplerBinding           = 0U;
            u32 deltaBinding             = 0U;
            u32 scatteringBinding        = 0U;
            if (!FindRequiredBinding(pass.mBindings, TEXT("AtmosphereParams"),
                    ERhiBindingType::ConstantBuffer, cbBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("TransmittanceLut"),
                    ERhiBindingType::SampledTexture, transBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("ScatteringDensityLut"),
                    ERhiBindingType::SampledTexture, scatteringDensityBinding)
                || !FindRequiredBinding(
                    pass.mBindings, TEXT("LinearSampler"), ERhiBindingType::Sampler, samplerBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("OutDeltaMultipleScattering"),
                    ERhiBindingType::StorageTexture, deltaBinding)
                || !FindRequiredBinding(pass.mBindings, TEXT("OutScattering"),
                    ERhiBindingType::StorageTexture, scatteringBinding)) {
                return false;
            }

            FBindGroupBuilder builder(pass.mLayout.Get());
            if (!builder.AddBuffer(
                    cbBinding, constantBuffer, 0ULL, sizeof(FAtmosphereComputeConstants))
                || !builder.AddTexture(transBinding, transmittanceTexture)
                || !builder.AddTexture(scatteringDensityBinding, scatteringDensityTexture)
                || !builder.AddSampler(samplerBinding, sampler)
                || !builder.AddStorageTexture(deltaBinding, deltaMultipleScatteringTexture)
                || !builder.AddStorageTexture(scatteringBinding, scatteringTexture)) {
                return false;
            }

            Rhi::FRhiBindGroupDesc groupDesc{};
            groupDesc.mDebugName.Assign(TEXT("Atmosphere.MultipleScattering.BindGroup"));
            if (!builder.Build(groupDesc)) {
                return false;
            }
            outGroup = device.CreateBindGroup(groupDesc);
            return outGroup.Get() != nullptr;
        }
    } // namespace

    struct FAtmosphereSystem::FGpuState {
        FAtmosphereComputePassResources mTransmittancePass;
        FAtmosphereComputePassResources mIrradiancePass;
        FAtmosphereComputePassResources mSingleScatteringPass;
        FAtmosphereComputePassResources mScatteringDensityPass;
        FAtmosphereComputePassResources mIndirectIrradiancePass;
        FAtmosphereComputePassResources mMultipleScatteringPass;
        Rhi::FRhiSamplerRef             mLinearSampler;
        Rhi::FRhiBufferRef              mConstantBuffer;

        [[nodiscard]] auto              IsValid() const noexcept -> bool {
            return mTransmittancePass.IsValid() && mIrradiancePass.IsValid()
                && mSingleScatteringPass.IsValid() && mScatteringDensityPass.IsValid()
                && mIndirectIrradiancePass.IsValid() && mMultipleScatteringPass.IsValid()
                && mLinearSampler && mConstantBuffer;
        }
    };

    FAtmosphereSystem::FAtmosphereSystem() : mGpuState(MakeUnique<FGpuState>()) {}

    auto FAtmosphereSystem::Get() noexcept -> FAtmosphereSystem& {
        static FAtmosphereSystem system;
        return system;
    }

    auto FAtmosphereSystem::EnsureSkyResources(const FAtmosphereSkyDesc& desc,
        const FVector3f& sunDirection) -> const FAtmosphereSkyResources* {
        auto* device = Rhi::RHIGetDevice();
        if (device == nullptr || mGpuState == nullptr) {
            return nullptr;
        }

        if (!mGpuState->IsValid()) {
            const auto transmittanceShaderPath = FindBuiltinShaderPath(kTransmittanceShaderFile);
            const auto irradianceShaderPath    = FindBuiltinShaderPath(kIrradianceShaderFile);
            const auto singleScatteringShaderPath =
                FindBuiltinShaderPath(kSingleScatteringShaderFile);
            const auto scatteringDensityShaderPath =
                FindBuiltinShaderPath(kScatteringDensityShaderFile);
            const auto indirectIrradianceShaderPath =
                FindBuiltinShaderPath(kIndirectIrradianceShaderFile);
            const auto multipleScatteringShaderPath =
                FindBuiltinShaderPath(kMultipleScatteringShaderFile);
            if (transmittanceShaderPath.IsEmpty() || irradianceShaderPath.IsEmpty()
                || singleScatteringShaderPath.IsEmpty() || scatteringDensityShaderPath.IsEmpty()
                || indirectIrradianceShaderPath.IsEmpty()
                || multipleScatteringShaderPath.IsEmpty()) {
                LogError(TEXT("Atmosphere compute shader sources not found."));
                return nullptr;
            }

            Rhi::FRhiShaderRef transmittanceShader{};
            Rhi::FRhiShaderRef irradianceShader{};
            Rhi::FRhiShaderRef singleScatteringShader{};
            Rhi::FRhiShaderRef scatteringDensityShader{};
            Rhi::FRhiShaderRef indirectIrradianceShader{};
            Rhi::FRhiShaderRef multipleScatteringShader{};
            if (!CompileShaderFromFile(*device, transmittanceShaderPath,
                    TEXT("CSAtmosphereTransmittance"), EShaderStage::Compute, transmittanceShader)
                || !CompileShaderFromFile(*device, irradianceShaderPath,
                    TEXT("CSAtmosphereIrradiance"), EShaderStage::Compute, irradianceShader)
                || !CompileShaderFromFile(*device, singleScatteringShaderPath,
                    TEXT("CSAtmosphereSingleScattering"), EShaderStage::Compute,
                    singleScatteringShader)
                || !CompileShaderFromFile(*device, scatteringDensityShaderPath,
                    TEXT("CSAtmosphereScatteringDensity"), EShaderStage::Compute,
                    scatteringDensityShader)
                || !CompileShaderFromFile(*device, indirectIrradianceShaderPath,
                    TEXT("CSAtmosphereIndirectIrradiance"), EShaderStage::Compute,
                    indirectIrradianceShader)
                || !CompileShaderFromFile(*device, multipleScatteringShaderPath,
                    TEXT("CSAtmosphereMultipleScattering"), EShaderStage::Compute,
                    multipleScatteringShader)
                || !BuildComputePass(*device, transmittanceShader, TEXT("Atmosphere.Transmittance"),
                    mGpuState->mTransmittancePass)
                || !BuildComputePass(*device, irradianceShader, TEXT("Atmosphere.Irradiance"),
                    mGpuState->mIrradiancePass)
                || !BuildComputePass(*device, singleScatteringShader,
                    TEXT("Atmosphere.SingleScattering"), mGpuState->mSingleScatteringPass)
                || !BuildComputePass(*device, scatteringDensityShader,
                    TEXT("Atmosphere.ScatteringDensity"), mGpuState->mScatteringDensityPass)
                || !BuildComputePass(*device, indirectIrradianceShader,
                    TEXT("Atmosphere.IndirectIrradiance"), mGpuState->mIndirectIrradiancePass)
                || !BuildComputePass(*device, multipleScatteringShader,
                    TEXT("Atmosphere.MultipleScattering"), mGpuState->mMultipleScatteringPass)) {
                return nullptr;
            }

            Rhi::FRhiSamplerDesc samplerDesc{};
            samplerDesc.mDebugName.Assign(TEXT("Atmosphere.LinearSampler"));
            samplerDesc.mFilter        = Rhi::ERhiSamplerFilter::Linear;
            samplerDesc.mAddressU      = Rhi::ERhiSamplerAddressMode::Clamp;
            samplerDesc.mAddressV      = Rhi::ERhiSamplerAddressMode::Clamp;
            samplerDesc.mAddressW      = Rhi::ERhiSamplerAddressMode::Clamp;
            mGpuState->mLinearSampler  = device->CreateSampler(samplerDesc);
            mGpuState->mConstantBuffer = CreateAtmosphereConstantBuffer(*device);
            if (!mGpuState->mLinearSampler || !mGpuState->mConstantBuffer) {
                return nullptr;
            }
        }

        const FVector3f clampedSunDirection = ClampDir(sunDirection);
        const bool      bNeedsRebuild       = !mHasCache || !(mCachedDesc.mVersion == desc.mVersion)
            || !IsSameVector(mCachedDesc.mRayleighScattering, desc.mRayleighScattering)
            || !IsSameVector(mCachedDesc.mMieScattering, desc.mMieScattering)
            || !IsSameVector(mCachedDesc.mMieAbsorption, desc.mMieAbsorption)
            || !IsSameVector(mCachedDesc.mOzoneAbsorption, desc.mOzoneAbsorption)
            || !IsSameVector(mCachedDesc.mGroundAlbedo, desc.mGroundAlbedo)
            || !IsSameVector(mCachedDesc.mSolarTint, desc.mSolarTint)
            || !IsSameVector(mCachedSunDirection, clampedSunDirection)
            || (mCachedDesc.mRayleighScaleHeightKm != desc.mRayleighScaleHeightKm)
            || (mCachedDesc.mMieScaleHeightKm != desc.mMieScaleHeightKm)
            || (mCachedDesc.mMieAnisotropy != desc.mMieAnisotropy)
            || (mCachedDesc.mOzoneCenterHeightKm != desc.mOzoneCenterHeightKm)
            || (mCachedDesc.mOzoneThicknessKm != desc.mOzoneThicknessKm)
            || (mCachedDesc.mSolarIlluminance != desc.mSolarIlluminance)
            || (mCachedDesc.mSunAngularRadius != desc.mSunAngularRadius)
            || (mCachedDesc.mPlanetRadiusKm != desc.mPlanetRadiusKm)
            || (mCachedDesc.mAtmosphereHeightKm != desc.mAtmosphereHeightKm)
            || (mCachedDesc.mViewHeightKm != desc.mViewHeightKm)
            || (mCachedDesc.mExposure != desc.mExposure);
        if (!bNeedsRebuild && mResources.IsValid()) {
            return &mResources;
        }

        mResources                         = {};
        mResources.mParamsBuffer           = mGpuState->mConstantBuffer;
        mResources.mTransmittanceLut       = CreateTexture(TEXT("Atmosphere.Transmittance"),
                  ERhiTextureDimension::Tex2D, kTransmittanceWidth, kTransmittanceHeight, 1U);
        mResources.mIrradianceLut          = CreateTexture(TEXT("Atmosphere.Irradiance"),
                     ERhiTextureDimension::Tex2D, kIrradianceWidth, kIrradianceHeight, 1U);
        mResources.mScatteringLut          = CreateTexture(TEXT("Atmosphere.Scattering"),
                     ERhiTextureDimension::Tex3D, kScatteringWidth, kScatteringHeight, kScatteringDepth);
        mResources.mSingleMieScatteringLut = CreateTexture(TEXT("Atmosphere.SingleMieScattering"),
            ERhiTextureDimension::Tex3D, kScatteringWidth, kScatteringHeight, kScatteringDepth);
        auto scatteringDensityLut          = CreateTexture(TEXT("Atmosphere.ScatteringDensity"),
                     ERhiTextureDimension::Tex3D, kScatteringWidth, kScatteringHeight, kScatteringDepth);
        auto deltaMultipleScatteringLut = CreateTexture(TEXT("Atmosphere.DeltaMultipleScattering"),
            ERhiTextureDimension::Tex3D, kScatteringWidth, kScatteringHeight, kScatteringDepth);
        auto deltaIrradianceLut         = CreateTexture(TEXT("Atmosphere.DeltaIrradiance"),
                    ERhiTextureDimension::Tex2D, kIrradianceWidth, kIrradianceHeight, 1U);
        auto singleRayleighScatteringLut =
            CreateTexture(TEXT("Atmosphere.SingleRayleighScattering"), ERhiTextureDimension::Tex3D,
                kScatteringWidth, kScatteringHeight, kScatteringDepth);

        if (!mResources.IsValid() || !scatteringDensityLut || !deltaMultipleScatteringLut
            || !deltaIrradianceLut || !singleRayleighScatteringLut) {
            mResources = {};
            return nullptr;
        }

        Rhi::FRhiCommandContextDesc ctxDesc{};
        ctxDesc.mQueueType = Rhi::ERhiQueueType::Graphics;
        ctxDesc.mDebugName.Assign(TEXT("Atmosphere.Precompute"));
        auto commandContext = device->CreateCommandContext(ctxDesc);
        if (!commandContext) {
            mResources = {};
            return nullptr;
        }

        auto* ops = dynamic_cast<Rhi::IRhiCmdContextOps*>(commandContext.Get());
        if (ops == nullptr) {
            mResources = {};
            return nullptr;
        }
        Rhi::FRhiCmdContextAdapter cmd(*commandContext.Get(), *ops);

        auto                       updateConstants = [&](u32 scatteringOrder) -> void {
            auto constants        = BuildConstants(desc, clampedSunDirection);
            constants.mOutput.Y() = static_cast<f32>(scatteringOrder);
            cmd.RHIUpdateDynamicBufferDiscard(
                mGpuState->mConstantBuffer.Get(), &constants, sizeof(constants), 0ULL);
        };

        auto DispatchTransmittance = [&]() -> bool {
            Rhi::FRhiBindGroupRef bindGroup{};
            if (!BuildTransmittanceBindGroup(*device, mGpuState->mTransmittancePass,
                    mGpuState->mConstantBuffer.Get(), mResources.mTransmittanceLut.Get(),
                    bindGroup)) {
                return false;
            }
            cmd.RHISetComputePipeline(mGpuState->mTransmittancePass.mPipeline.Get());
            cmd.RHISetBindGroup(0U, bindGroup.Get(), nullptr, 0U);
            cmd.RHIDispatch(DivideRoundUp(kTransmittanceWidth, kThreadGroupSizeX),
                DivideRoundUp(kTransmittanceHeight, kThreadGroupSizeY), 1U);
            return true;
        };

        auto DispatchIrradiance = [&]() -> bool {
            Rhi::FRhiBindGroupRef bindGroup{};
            if (!BuildIrradianceBindGroup(*device, mGpuState->mIrradiancePass,
                    mGpuState->mConstantBuffer.Get(), mResources.mTransmittanceLut.Get(),
                    mGpuState->mLinearSampler.Get(), mResources.mIrradianceLut.Get(), bindGroup)) {
                return false;
            }
            cmd.RHISetComputePipeline(mGpuState->mIrradiancePass.mPipeline.Get());
            cmd.RHISetBindGroup(0U, bindGroup.Get(), nullptr, 0U);
            cmd.RHIDispatch(DivideRoundUp(kIrradianceWidth, kThreadGroupSizeX),
                DivideRoundUp(kIrradianceHeight, kThreadGroupSizeY), 1U);
            return true;
        };

        auto DispatchSingleScattering = [&]() -> bool {
            Rhi::FRhiBindGroupRef bindGroup{};
            if (!BuildSingleScatteringBindGroup(*device, mGpuState->mSingleScatteringPass,
                    mGpuState->mConstantBuffer.Get(), mResources.mTransmittanceLut.Get(),
                    mGpuState->mLinearSampler.Get(), singleRayleighScatteringLut.Get(),
                    mResources.mScatteringLut.Get(), mResources.mSingleMieScatteringLut.Get(),
                    bindGroup)) {
                return false;
            }
            cmd.RHISetComputePipeline(mGpuState->mSingleScatteringPass.mPipeline.Get());
            cmd.RHISetBindGroup(0U, bindGroup.Get(), nullptr, 0U);
            cmd.RHIDispatch(DivideRoundUp(kScatteringWidth, kThreadGroupSizeX),
                DivideRoundUp(kScatteringHeight, kThreadGroupSizeY), kScatteringDepth);
            return true;
        };

        auto DispatchScatteringDensity = [&](Rhi::FRhiTexture* multipleScatteringTexture) -> bool {
            Rhi::FRhiBindGroupRef bindGroup{};
            if (!BuildScatteringDensityBindGroup(*device, mGpuState->mScatteringDensityPass,
                    mGpuState->mConstantBuffer.Get(), mResources.mTransmittanceLut.Get(),
                    singleRayleighScatteringLut.Get(), mResources.mSingleMieScatteringLut.Get(),
                    multipleScatteringTexture, mResources.mIrradianceLut.Get(),
                    mGpuState->mLinearSampler.Get(), scatteringDensityLut.Get(), bindGroup)) {
                return false;
            }
            cmd.RHISetComputePipeline(mGpuState->mScatteringDensityPass.mPipeline.Get());
            cmd.RHISetBindGroup(0U, bindGroup.Get(), nullptr, 0U);
            cmd.RHIDispatch(DivideRoundUp(kScatteringWidth, kThreadGroupSizeX),
                DivideRoundUp(kScatteringHeight, kThreadGroupSizeY), kScatteringDepth);
            return true;
        };

        auto DispatchIndirectIrradiance = [&](Rhi::FRhiTexture* multipleScatteringTexture) -> bool {
            Rhi::FRhiBindGroupRef bindGroup{};
            if (!BuildIndirectIrradianceBindGroup(*device, mGpuState->mIndirectIrradiancePass,
                    mGpuState->mConstantBuffer.Get(), singleRayleighScatteringLut.Get(),
                    mResources.mSingleMieScatteringLut.Get(), multipleScatteringTexture,
                    mResources.mIrradianceLut.Get(), mGpuState->mLinearSampler.Get(),
                    deltaIrradianceLut.Get(), bindGroup)) {
                return false;
            }
            cmd.RHISetComputePipeline(mGpuState->mIndirectIrradiancePass.mPipeline.Get());
            cmd.RHISetBindGroup(0U, bindGroup.Get(), nullptr, 0U);
            cmd.RHIDispatch(DivideRoundUp(kIrradianceWidth, kThreadGroupSizeX),
                DivideRoundUp(kIrradianceHeight, kThreadGroupSizeY), 1U);
            return true;
        };

        auto DispatchMultipleScattering = [&]() -> bool {
            Rhi::FRhiBindGroupRef bindGroup{};
            if (!BuildMultipleScatteringBindGroup(*device, mGpuState->mMultipleScatteringPass,
                    mGpuState->mConstantBuffer.Get(), mResources.mTransmittanceLut.Get(),
                    scatteringDensityLut.Get(), mGpuState->mLinearSampler.Get(),
                    deltaMultipleScatteringLut.Get(), mResources.mScatteringLut.Get(), bindGroup)) {
                return false;
            }
            cmd.RHISetComputePipeline(mGpuState->mMultipleScatteringPass.mPipeline.Get());
            cmd.RHISetBindGroup(0U, bindGroup.Get(), nullptr, 0U);
            cmd.RHIDispatch(DivideRoundUp(kScatteringWidth, kThreadGroupSizeX),
                DivideRoundUp(kScatteringHeight, kThreadGroupSizeY), kScatteringDepth);
            return true;
        };

        updateConstants(0U);
        if (!DispatchTransmittance() || !DispatchIrradiance() || !DispatchSingleScattering()) {
            mResources = {};
            return nullptr;
        }

        Rhi::FRhiTexture* previousOrderMultipleScattering = deltaMultipleScatteringLut.Get();
        for (u32 scatteringOrder = 2U; scatteringOrder <= kMaxScatteringOrder; ++scatteringOrder) {
            updateConstants(scatteringOrder);
            if (!DispatchScatteringDensity(previousOrderMultipleScattering)
                || !DispatchIndirectIrradiance(previousOrderMultipleScattering)
                || !DispatchMultipleScattering()) {
                mResources = {};
                return nullptr;
            }
            previousOrderMultipleScattering = deltaMultipleScatteringLut.Get();
        }

        commandContext->RHIFlushContextDevice({});

        mResources.mTransmittanceLutSrv = BuildSrv(*device, mResources.mTransmittanceLut.Get(),
            ERhiFormat::R16G16B16A16Float, 1U, 1U, 1U, TEXT("Atmosphere.Transmittance.SRV"));
        mResources.mIrradianceLutSrv    = BuildSrv(*device, mResources.mIrradianceLut.Get(),
               ERhiFormat::R16G16B16A16Float, 1U, 1U, 1U, TEXT("Atmosphere.Irradiance.SRV"));
        mResources.mScatteringLutSrv =
            BuildSrv(*device, mResources.mScatteringLut.Get(), ERhiFormat::R16G16B16A16Float, 1U,
                1U, kScatteringDepth, TEXT("Atmosphere.Scattering.SRV"));
        mResources.mSingleMieScatteringLutSrv = BuildSrv(*device,
            mResources.mSingleMieScatteringLut.Get(), ERhiFormat::R16G16B16A16Float, 1U, 1U,
            kScatteringDepth, TEXT("Atmosphere.SingleMieScattering.SRV"));

        mCachedDesc         = desc;
        mCachedSunDirection = clampedSunDirection;
        mHasCache           = mResources.IsValid();
        return mHasCache ? &mResources : nullptr;
    }

    auto FAtmosphereSystem::GetResources() const noexcept -> const FAtmosphereSkyResources* {
        return mResources.IsValid() ? &mResources : nullptr;
    }

    void FAtmosphereSystem::Reset() noexcept {
        mResources          = {};
        mCachedDesc         = {};
        mCachedSunDirection = FVector3f(0.0f, 1.0f, 0.0f);
        mGpuState           = MakeUnique<FGpuState>();
        mHasCache           = false;
    }
} // namespace AltinaEngine::Rendering::Atmosphere
