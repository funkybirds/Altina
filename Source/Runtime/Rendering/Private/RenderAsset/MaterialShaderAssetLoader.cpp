#include "RenderAsset/MaterialShaderAssetLoader.h"

#include "Algorithm/CStringUtils.h"
#include "Asset/ShaderAsset.h"
#include "Container/String.h"
#include "Logging/Log.h"
#include "Material/Material.h"
#include "Asset/AssetBinary.h"
#include "Asset/Texture2DAsset.h"
#include "Rhi/RhiInit.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Platform/PlatformFileSystem.h"
#include "Rendering/BasicDeferredRenderer.h"
#include "Shader/ShaderPreset.h"
#include "Shader/ShaderPermutation.h"
#include "Shader/ShaderReflection.h"
#include "ShaderCompiler/ShaderPermutationParser.h"
#include "ShaderCompiler/ShaderCompiler.h"
#include "ShaderCompiler/ShaderRhiBindings.h"
#include "Threading/Mutex.h"
#include "Types/Traits.h"
#include "Utility/String/CodeConvert.h"
#include "Utility/Filesystem/Path.h"
#include "Utility/Filesystem/PathUtils.h"

#include <fstream>

using namespace AltinaEngine;

namespace AltinaEngine::Rendering {
    namespace {
        namespace Container = Core::Container;
        using Container::FString;
        using Container::FStringView;
        using Core::Threading::FMutex;
        using Core::Threading::FScopedLock;
        using Core::Utility::Filesystem::FPath;

        struct FTextureSrvCacheKey {
            Asset::FAssetHandle mHandle{};

            [[nodiscard]] auto operator==(const FTextureSrvCacheKey& other) const noexcept -> bool {
                return mHandle == other.mHandle;
            }
        };

        struct FTextureSrvCacheKeyHash {
            auto operator()(const FTextureSrvCacheKey& key) const noexcept -> usize {
                constexpr u64 kFnvOffset64 = 1469598103934665603ULL;
                constexpr u64 kFnvPrime64  = 1099511628211ULL;

                u64           hash  = kFnvOffset64;
                const auto*   bytes = key.mHandle.mUuid.Data();
                for (usize i = 0U; i < FUuid::kByteCount; ++i) {
                    hash ^= bytes[i];
                    hash *= kFnvPrime64;
                }
                hash ^= static_cast<u8>(key.mHandle.mType);
                hash *= kFnvPrime64;
                return static_cast<usize>(hash);
            }
        };

        struct FTextureSrvCache {
            FMutex mMutex{};
            Container::THashMap<FTextureSrvCacheKey, Rhi::FRhiShaderResourceViewRef,
                FTextureSrvCacheKeyHash>
                mEntries;
        };

        auto GetTextureSrvCache() -> FTextureSrvCache& {
            static FTextureSrvCache sCache{};
            return sCache;
        }

        void ClearTextureSrvCache() noexcept {
            auto&       cache = GetTextureSrvCache();
            FScopedLock lock(cache.mMutex);
            cache.mEntries.Clear();
        }

        [[nodiscard]] auto ResolveShaderTargetBackend() noexcept -> Rhi::ERhiBackend {
            const auto backend = Rhi::RHIGetBackend();
            return (backend != Rhi::ERhiBackend::Unknown) ? backend : Rhi::ERhiBackend::DirectX11;
        }

        auto TryParseMaterialPass(FStringView name, RenderCore::EMaterialPass& outPass) -> bool {
            auto EqualsI = [](FStringView lhs, const TChar* rhs) -> bool {
                if (rhs == nullptr) {
                    return false;
                }
                const FStringView rhsView(rhs);
                if (lhs.Length() != rhsView.Length()) {
                    return false;
                }
                for (usize i = 0; i < lhs.Length(); ++i) {
                    if (Core::Algorithm::ToLowerChar(lhs[i])
                        != Core::Algorithm::ToLowerChar(rhsView[i])) {
                        return false;
                    }
                }
                return true;
            };

            if (EqualsI(name, TEXT("BasePass"))) {
                outPass = RenderCore::EMaterialPass::BasePass;
                return true;
            }
            if (EqualsI(name, TEXT("DepthPass"))) {
                outPass = RenderCore::EMaterialPass::DepthPass;
                return true;
            }
            if (EqualsI(name, TEXT("ShadowPass"))) {
                outPass = RenderCore::EMaterialPass::ShadowPass;
                return true;
            }
            return false;
        }

        void ApplyRasterOverrides(const Asset::FMaterialRasterStateOverrides& overrides,
            Rhi::FRhiRasterStateDesc&                                         outRaster) noexcept {
            if (overrides.mHasFillMode) {
                switch (overrides.mFillMode) {
                    case Asset::EMaterialRasterFillMode::Wireframe:
                        outRaster.mFillMode = Rhi::ERhiRasterFillMode::Wireframe;
                        break;
                    case Asset::EMaterialRasterFillMode::Solid:
                    default:
                        outRaster.mFillMode = Rhi::ERhiRasterFillMode::Solid;
                        break;
                }
            }

            if (overrides.mHasCullMode) {
                switch (overrides.mCullMode) {
                    case Asset::EMaterialRasterCullMode::None:
                        outRaster.mCullMode = Rhi::ERhiRasterCullMode::None;
                        break;
                    case Asset::EMaterialRasterCullMode::Front:
                        outRaster.mCullMode = Rhi::ERhiRasterCullMode::Front;
                        break;
                    case Asset::EMaterialRasterCullMode::Back:
                    default:
                        outRaster.mCullMode = Rhi::ERhiRasterCullMode::Back;
                        break;
                }
            }

            if (overrides.mHasFrontFace) {
                switch (overrides.mFrontFace) {
                    case Asset::EMaterialRasterFrontFace::CW:
                        outRaster.mFrontFace = Rhi::ERhiRasterFrontFace::CW;
                        break;
                    case Asset::EMaterialRasterFrontFace::CCW:
                    default:
                        outRaster.mFrontFace = Rhi::ERhiRasterFrontFace::CCW;
                        break;
                }
            }

            if (overrides.mHasDepthBias) {
                outRaster.mDepthBias = overrides.mDepthBias;
            }
            if (overrides.mHasDepthBiasClamp) {
                outRaster.mDepthBiasClamp = overrides.mDepthBiasClamp;
            }
            if (overrides.mHasSlopeScaledDepthBias) {
                outRaster.mSlopeScaledDepthBias = overrides.mSlopeScaledDepthBias;
            }
            if (overrides.mHasDepthClip) {
                outRaster.mDepthClip = overrides.mDepthClip;
            }
            if (overrides.mHasConservativeRaster) {
                outRaster.mConservativeRaster = overrides.mConservativeRaster;
            }
        }

        auto IsMaterialCBufferName(FStringView name) -> bool {
            if (name.IsEmpty()) {
                return false;
            }
            const FStringView target(TEXT("MaterialConstants"));
            if (name.Length() < target.Length()) {
                return false;
            }
            for (usize i = 0U; i < target.Length(); ++i) {
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

        void LogReflectionResources(
            const Shader::FShaderReflection* reflection, const FStringView& label) {
            // Too chatty in normal runs; enable when diagnosing shader reflection issues.
            // if (reflection == nullptr) {
            //     LogInfo(TEXT("ShaderReflection {}: <null>"), label.Data());
            //     return;
            // }
            // LogInfo(TEXT("ShaderReflection {} resources={} cbuffers={}"), label.Data(),
            //     static_cast<u32>(reflection->mResources.Size()),
            //     static_cast<u32>(reflection->mConstantBuffers.Size()));
            // for (const auto& resource : reflection->mResources) {
            //     LogInfo(TEXT("  Resource name={} type={} set={} binding={} reg={} space={}"),
            //         resource.mName.CStr(), static_cast<u32>(resource.mType), resource.mSet,
            //         resource.mBinding, resource.mRegister, resource.mSpace);
            // }
            // for (const auto& cbuffer : reflection->mConstantBuffers) {
            //     LogInfo(TEXT("  CBuffer name={} set={} binding={} reg={} space={} size={}"),
            //         cbuffer.mName.CStr(), cbuffer.mSet, cbuffer.mBinding, cbuffer.mRegister,
            //         cbuffer.mSpace, cbuffer.mSizeBytes);
            // }
            (void)reflection;
            (void)label;
        }

        auto BuildMaterialLayout(const Shader::FShaderReflection* vertex,
            const Shader::FShaderReflection* pixel) -> RenderCore::FMaterialLayout {
            RenderCore::FMaterialLayout layout;
            const auto*                 materialCBuffer = SelectMaterialCBuffer(vertex, pixel);
            if (materialCBuffer == nullptr) {
                return layout;
            }
            LogReflectionResources(vertex, TEXT("Vertex"));
            LogReflectionResources(pixel, TEXT("Pixel"));
            layout.InitFromConstantBuffer(*materialCBuffer);
            // Prefer PS texture bindings for material layout. VS bindings can alias the same
            // backend binding index (notably D3D11 register flattening) and cause false
            // conflicts; only fall back to VS when PS has no texture bindings.
            AddTextureBindings(layout, pixel);
            if (layout.mTextureBindings.IsEmpty()) {
                AddTextureBindings(layout, vertex);
            }
            layout.SortTextureBindings();
            return layout;
        }

        void LogMaterialLayout(const RenderCore::FMaterialLayout& layout,
            const Shader::FShaderConstantBuffer* materialCBuffer, const FString& passName) {
            // Too chatty in normal runs; enable when diagnosing material layout issues.
            // LogInfo(TEXT("Material Layout for pass {}"), passName.CStr());
            // if (!layout.PropertyBag.IsValid()) {
            //     LogInfo(TEXT("  PropertyBag: <invalid>"));
            // } else {
            //     LogInfo(
            //         TEXT("  PropertyBag: Name={} Size={} Set={} Binding={} Register={}
            //         Space={}"), layout.PropertyBag.GetName().CStr(),
            //         layout.PropertyBag.GetSizeBytes(), layout.PropertyBag.GetSet(),
            //         layout.PropertyBag.GetBinding(), layout.PropertyBag.GetRegister(),
            //         layout.PropertyBag.GetSpace());
            // }
            // if (materialCBuffer == nullptr) {
            //     LogWarning(TEXT("  Material CBuffer: <null>"));
            // } else {
            //     LogInfo(
            //         TEXT(
            //             "  Material CBuffer: Name={} Size={} Set={} Binding={} Register={}
            //             Space={}"),
            //         materialCBuffer->mName.CStr(), materialCBuffer->mSizeBytes,
            //         materialCBuffer->mSet, materialCBuffer->mBinding, materialCBuffer->mRegister,
            //         materialCBuffer->mSpace);
            //     LogInfo(
            //         TEXT("  Properties: {}"),
            //         static_cast<u32>(materialCBuffer->mMembers.Size()));
            //     for (const auto& member : materialCBuffer->mMembers) {
            //         const auto nameHash =
            //         RenderCore::HashMaterialParamName(member.mName.ToView()); LogInfo(
            //             TEXT("    {} (hash=0x{:08X}) Offset={} Size={} ElemCount={}
            //             ElemStride={}"), member.mName.CStr(), nameHash, member.mOffset,
            //             member.mSize, member.mElementCount, member.mElementStride);
            //     }
            // }
            // const usize textureCount = layout.TextureBindings.Size();
            // LogInfo(TEXT("  TextureBindings: {}"), static_cast<u32>(textureCount));
            // for (usize i = 0U; i < textureCount; ++i) {
            //     const u32 nameHash =
            //         (i < layout.TextureNameHashes.Size()) ? layout.TextureNameHashes[i] : 0U;
            //     const u32 samplerBinding = (i < layout.SamplerBindings.Size())
            //         ? layout.SamplerBindings[i]
            //         : RenderCore::kMaterialInvalidBinding;
            //     LogInfo(TEXT("    [{}] NameHash=0x{:08X} TextureBinding={} SamplerBinding={}"),
            //         static_cast<u32>(i), nameHash, layout.TextureBindings[i], samplerBinding);
            // }
            (void)layout;
            (void)materialCBuffer;
            (void)passName;
        }

        auto WriteTempShaderFile(const Container::FNativeStringView& source, const FUuid& uuid,
            ShaderCompiler::EShaderSourceLanguage language,
            Core::Utility::Filesystem::FPath&     outPath) -> bool {
            auto tempRoot = Core::Utility::Filesystem::GetTempDirectory();
            if (tempRoot.IsEmpty()) {
                return false;
            }
            tempRoot /= TEXT("AltinaEngine");
            tempRoot /= TEXT("Shaders");
            if (!Core::Platform::CreateDirectories(tempRoot.GetString())) {
                return false;
            }

            const auto               uuidText = uuid.ToNativeString();
            Container::FNativeString fileName(uuidText.GetData(), uuidText.Length());
            fileName.Append(
                (language == ShaderCompiler::EShaderSourceLanguage::Slang) ? ".slang" : ".hlsl");
            const auto fileNameText = Core::Utility::String::FromUtf8(fileName);
            outPath                 = tempRoot / fileNameText;

            const auto    outPathUtf8 = Core::Utility::String::ToUtf8Bytes(outPath.GetString());
            std::ofstream file(outPathUtf8.CStr(), std::ios::binary | std::ios::trunc);
            if (!file.good()) {
                return false;
            }
            if (source.Length() > 0) {
                file.write(source.Data(), static_cast<std::streamsize>(source.Length()));
            }
            return file.good();
        }

        auto ResolveShaderPath(FStringView relativePath) -> FPath {
            if (relativePath.IsEmpty()) {
                return {};
            }

            FPath relPath(relativePath);
            if (relPath.IsAbsolute() && relPath.Exists()) {
                return relPath;
            }

            const FPath exeDir(Core::Platform::GetExecutableDir());
            if (!exeDir.IsEmpty()) {
                const auto candidate = exeDir / relativePath;
                if (candidate.Exists()) {
                    return candidate;
                }
            }

            const auto cwd = Core::Utility::Filesystem::GetCurrentWorkingDir();
            if (!cwd.IsEmpty()) {
                const auto candidate = cwd / relativePath;
                if (candidate.Exists()) {
                    return candidate;
                }

                FString sourceRel(TEXT("Source/"));
                sourceRel.Append(relativePath);
                const auto candidate2 = cwd / sourceRel;
                if (candidate2.Exists()) {
                    return candidate2;
                }
            }

            FString sourceRel(TEXT("Source/"));
            sourceRel.Append(relativePath);
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

        auto CompileShaderFromFile(const FPath& path, FStringView entry, Shader::EShaderStage stage,
            const FStringView keyPrefix, RenderCore::FShaderRegistry::FShaderKey& outKey,
            ShaderCompiler::FShaderCompileResult& outResult) -> bool {
            if (path.IsEmpty() || !path.Exists()) {
                LogError(TEXT("Preset shader source not found."));
                return false;
            }

            ShaderCompiler::FShaderCompileRequest request{};
            request.mSource.mPath.Assign(path.GetString().ToView());
            request.mSource.mEntryPoint.Assign(entry);
            request.mSource.mStage    = stage;
            request.mSource.mLanguage = ShaderCompiler::EShaderSourceLanguage::Hlsl;

            FPath includeDir = path.ParentPath();
            FPath probe      = includeDir;
            for (u32 i = 0U; i < 8U && !probe.IsEmpty(); ++i) {
                if (probe.Filename() == TEXT("Shader")) {
                    includeDir = probe.ParentPath();
                    break;
                }
                const auto parent = probe.ParentPath();
                if (parent == probe) {
                    break;
                }
                probe = parent;
            }
            if (!includeDir.IsEmpty()) {
                request.mSource.mIncludeDirs.PushBack(includeDir.GetString());
            }

            request.mOptions.mTargetBackend = ResolveShaderTargetBackend();
            request.mOptions.mOptimization  = ShaderCompiler::EShaderOptimization::Default;
            request.mOptions.mDebugInfo     = false;

            outResult = ShaderCompiler::GetShaderCompiler().Compile(request);
            if (!outResult.mSucceeded) {
                LogError(TEXT("Preset shader compile failed: {}"), outResult.mDiagnostics.ToView());
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
                LogError(TEXT("Failed to create preset RHI shader."));
                return false;
            }

            FString keyName(keyPrefix);
            keyName.Append(TEXT("."));
            keyName.Append(entry);
            outKey = RenderCore::FShaderRegistry::MakeKey(keyName.ToView(), stage);

            if (!Rendering::FBasicDeferredRenderer::RegisterShader(outKey, shader)) {
                LogError(TEXT("Failed to register preset shader {}."), outKey.mName.ToView());
                return false;
            }

            return true;
        }

        auto SelectPresetEntry(RenderCore::EMaterialPass pass, Shader::EShaderStage stage)
            -> FStringView {
            if (pass == RenderCore::EMaterialPass::BasePass) {
                return (stage == Shader::EShaderStage::Vertex) ? FStringView(TEXT("VSBase"))
                                                               : FStringView(TEXT("PSBase"));
            }
            return {};
        }

        auto TryParseRasterState(const Asset::FShaderAsset& shader, Shader::FShaderRasterState& out)
            -> bool {
            ShaderCompiler::FShaderPermutationParseResult parse{};
            const auto                                    sourceText =
                Core::Utility::String::FromUtf8(Container::FNativeString(shader.GetSource()));
            if (!ShaderCompiler::ParseShaderPermutationSource(sourceText.ToView(), parse)) {
                return false;
            }
            if (!parse.mHasRasterState) {
                return false;
            }
            out = parse.mRasterState;
            return true;
        }

        auto TryParseRasterStateFromFile(const FPath& shaderPath, Shader::FShaderRasterState& out)
            -> bool {
            if (shaderPath.IsEmpty() || !shaderPath.Exists()) {
                return false;
            }

            Container::FNativeString source;
            if (!Core::Platform::ReadFileTextUtf8(shaderPath.GetString(), source)) {
                return false;
            }

            ShaderCompiler::FShaderPermutationParseResult parse{};
            const auto sourceText = Core::Utility::String::FromUtf8(source);
            if (!ShaderCompiler::ParseShaderPermutationSource(sourceText.ToView(), parse)) {
                return false;
            }
            if (!parse.mHasRasterState) {
                return false;
            }
            out = parse.mRasterState;
            return true;
        }

        auto ToRhiFormat(const Asset::FTexture2DDesc& desc) -> Rhi::ERhiFormat {
            const bool srgb = desc.SRGB;
            switch (desc.Format) {
                case Asset::kTextureFormatRGBA8:
                    return srgb ? Rhi::ERhiFormat::R8G8B8A8UnormSrgb
                                : Rhi::ERhiFormat::R8G8B8A8Unorm;
                case Asset::kTextureFormatR8:
                case Asset::kTextureFormatRGB8:
                default:
                    return srgb ? Rhi::ERhiFormat::R8G8B8A8UnormSrgb
                                : Rhi::ERhiFormat::R8G8B8A8Unorm;
            }
        }

        auto CreateTextureSrv(const Asset::FAssetHandle& handle,
            const Asset::FTexture2DAsset&                asset) -> Rhi::FRhiShaderResourceViewRef {
            if (handle.IsValid()) {
                auto& cache = GetTextureSrvCache();
                {
                    FScopedLock lock(cache.mMutex);
                    const auto  it = cache.mEntries.FindIt(FTextureSrvCacheKey{ handle });
                    if (it != cache.mEntries.end()) {
                        return it->second;
                    }
                }
            }

            auto* device = Rhi::RHIGetDevice();
            if (device == nullptr) {
                return {};
            }

            const auto&          assetDesc = asset.GetDesc();
            Rhi::FRhiTextureDesc texDesc{};
            texDesc.mWidth     = assetDesc.Width;
            texDesc.mHeight    = assetDesc.Height;
            texDesc.mMipLevels = (assetDesc.MipCount > 0U) ? assetDesc.MipCount : 1U;
            texDesc.mFormat    = ToRhiFormat(assetDesc);
            texDesc.mBindFlags = Rhi::ERhiTextureBindFlags::ShaderResource;

            auto texture = Rhi::RHICreateTexture(texDesc);
            if (!texture) {
                return {};
            }

            const auto& pixels = asset.GetPixels();
            if (!pixels.IsEmpty()) {
                const u32 bytesPerPixel = Asset::GetTextureBytesPerPixel(assetDesc.Format);
                u32       width         = assetDesc.Width;
                u32       height        = assetDesc.Height;
                usize     offset        = 0U;
                for (u32 mip = 0U; mip < texDesc.mMipLevels; ++mip) {
                    const usize rowPitch   = static_cast<usize>(width) * bytesPerPixel;
                    const usize slicePitch = rowPitch * static_cast<usize>(height);
                    if (rowPitch == 0U || slicePitch == 0U) {
                        break;
                    }
                    if (offset + slicePitch > pixels.Size()) {
                        break;
                    }
                    Rhi::FRhiTextureSubresource subresource{};
                    subresource.mMipLevel = mip;
                    device->UpdateTextureSubresource(texture.Get(), subresource,
                        pixels.Data() + offset, static_cast<u32>(rowPitch),
                        static_cast<u32>(slicePitch));
                    offset += slicePitch;
                    width  = (width > 1U) ? (width >> 1U) : 1U;
                    height = (height > 1U) ? (height >> 1U) : 1U;
                }
            }

            Rhi::FRhiShaderResourceViewDesc viewDesc{};
            viewDesc.mTexture                      = texture.Get();
            viewDesc.mFormat                       = texDesc.mFormat;
            viewDesc.mTextureRange.mBaseMip        = 0U;
            viewDesc.mTextureRange.mMipCount       = texDesc.mMipLevels;
            viewDesc.mTextureRange.mBaseArrayLayer = 0U;
            viewDesc.mTextureRange.mLayerCount     = texDesc.mArrayLayers;
            auto srv                               = device->CreateShaderResourceView(viewDesc);
            if (srv && handle.IsValid()) {
                auto&       cache = GetTextureSrvCache();
                FScopedLock lock(cache.mMutex);
                cache.mEntries.InsertOrAssign(FTextureSrvCacheKey{ handle }, srv);
            }
            return srv;
        }

        auto BuildTemplateOverrides(const Asset::FMeshMaterialParameterBlock& overrides)
            -> RenderCore::FMaterialParameterBlock {
            RenderCore::FMaterialParameterBlock block;
            for (const auto& scalar : overrides.GetScalars()) {
                block.SetScalar(scalar.mNameHash, scalar.mValue);
            }
            for (const auto& vector : overrides.GetVectors()) {
                block.SetVector(vector.mNameHash, vector.mValue);
            }
            for (const auto& matrix : overrides.GetMatrices()) {
                block.SetMatrix(matrix.mNameHash, matrix.mValue);
            }
            return block;
        }

        auto HasTemplateOverrides(const RenderCore::FMaterialParameterBlock& overrides) -> bool {
            return !overrides.GetScalars().IsEmpty() || !overrides.GetVectors().IsEmpty()
                || !overrides.GetMatrices().IsEmpty();
        }
    } // namespace

    auto CompileShaderFromAsset(const Asset::FAssetHandle& handle, FStringView entry,
        Shader::EShaderStage stage, Asset::FAssetRegistry& registry, Asset::FAssetManager& manager,
        RenderCore::FShaderRegistry::FShaderKey& outKey,
        ShaderCompiler::FShaderCompileResult&    outResult) -> bool {
        const auto* desc = registry.GetDesc(handle);
        if (desc == nullptr) {
            LogError(TEXT("Shader asset desc missing."));
            return false;
        }

        auto  asset       = manager.Load(handle);
        auto* shaderAsset = asset ? static_cast<Asset::FShaderAsset*>(asset.Get()) : nullptr;
        if (shaderAsset == nullptr) {
            LogError(TEXT("Failed to load shader asset."));
            return false;
        }

        ShaderCompiler::EShaderSourceLanguage language =
            ShaderCompiler::EShaderSourceLanguage::Hlsl;
        if (shaderAsset->GetLanguage() == Asset::kShaderLanguageSlang) {
            language = ShaderCompiler::EShaderSourceLanguage::Slang;
        }

        Core::Utility::Filesystem::FPath tempPath;
        if (!WriteTempShaderFile(shaderAsset->GetSource(), handle.mUuid, language, tempPath)) {
            LogError(TEXT("Failed to write temp shader file."));
            return false;
        }

        ShaderCompiler::FShaderCompileRequest request{};
        request.mSource.mPath.Assign(tempPath.GetString().ToView());
        request.mSource.mEntryPoint.Assign(entry);
        request.mSource.mStage    = stage;
        request.mSource.mLanguage = language;
        if (!tempPath.ParentPath().IsEmpty()) {
            request.mSource.mIncludeDirs.PushBack(tempPath.ParentPath().GetString());
        }
        request.mOptions.mTargetBackend = ResolveShaderTargetBackend();
        request.mOptions.mOptimization  = ShaderCompiler::EShaderOptimization::Default;
        request.mOptions.mDebugInfo     = false;

        auto& compiler = ShaderCompiler::GetShaderCompiler();
        outResult      = compiler.Compile(request);

        Core::Platform::RemoveFileIfExists(tempPath.GetString());

        if (!outResult.mSucceeded) {
            LogError(TEXT("Shader compile failed: {}"), outResult.mDiagnostics.ToView());
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
            LogError(TEXT("Failed to create RHI shader."));
            return false;
        }

        outKey =
            RenderCore::FShaderRegistry::MakeAssetKey(desc->mVirtualPath.ToView(), entry, stage);
        if (!Rendering::FBasicDeferredRenderer::RegisterShader(outKey, shader)) {
            LogError(TEXT("Failed to register shader for {}."), outKey.mName.ToView());
            return false;
        }
        return true;
    }

    auto BuildMaterialTemplateFromAsset(const Asset::FMaterialAsset& asset,
        Asset::FAssetRegistry& registry, Asset::FAssetManager& manager)
        -> Container::TShared<RenderCore::FMaterialTemplate> {
        auto templ = Container::MakeShared<RenderCore::FMaterialTemplate>();

        for (const auto& pass : asset.GetPasses()) {
            RenderCore::EMaterialPass passType{};
            if (!TryParseMaterialPass(pass.mName.ToView(), passType)) {
                continue;
            }

            RenderCore::FMaterialPassDesc        passDesc{};
            ShaderCompiler::FShaderCompileResult vertexResult{};
            ShaderCompiler::FShaderCompileResult pixelResult{};
            bool                                 hasVertexResult = false;
            bool                                 hasPixelResult  = false;
            Shader::FShaderRasterState           rasterState{};
            bool                                 hasRasterState = false;

            if (!pass.mPreset.IsEmptyString()) {
                const auto* presetPath =
                    RenderCore::FShaderPresetRegistry::FindPreset(pass.mPreset.ToView());
                if (presetPath == nullptr) {
                    LogError(TEXT("Shader preset not found: {}"), pass.mPreset.ToView());
                    return {};
                }

                const auto shaderPath = ResolveShaderPath(presetPath->ToView());
                hasRasterState        = TryParseRasterStateFromFile(shaderPath, rasterState);
                const auto vsEntry    = SelectPresetEntry(passType, Shader::EShaderStage::Vertex);
                const auto psEntry    = SelectPresetEntry(passType, Shader::EShaderStage::Pixel);
                if (vsEntry.IsEmpty() || psEntry.IsEmpty()) {
                    LogError(TEXT("Shader preset pass not supported: {}"), pass.mPreset.ToView());
                    return {};
                }

                RenderCore::FShaderRegistry::FShaderKey key{};
                const FStringView                       keyPrefix = pass.mPreset.ToView();
                if (!CompileShaderFromFile(shaderPath, vsEntry, Shader::EShaderStage::Vertex,
                        keyPrefix, key, vertexResult)) {
                    return {};
                }
                passDesc.mShaders.mVertex = key;
                hasVertexResult           = true;

                if (!CompileShaderFromFile(shaderPath, psEntry, Shader::EShaderStage::Pixel,
                        keyPrefix, key, pixelResult)) {
                    return {};
                }
                passDesc.mShaders.mPixel = key;
                hasPixelResult           = true;
            } else if (pass.mHasVertex) {
                RenderCore::FShaderRegistry::FShaderKey key{};
                if (!CompileShaderFromAsset(pass.mVertex.mAsset, pass.mVertex.mEntry.ToView(),
                        Shader::EShaderStage::Vertex, registry, manager, key, vertexResult)) {
                    return {};
                }
                passDesc.mShaders.mVertex = key;
                hasVertexResult           = true;
            }

            if (pass.mPreset.IsEmptyString() && pass.mHasPixel) {
                RenderCore::FShaderRegistry::FShaderKey key{};
                if (!CompileShaderFromAsset(pass.mPixel.mAsset, pass.mPixel.mEntry.ToView(),
                        Shader::EShaderStage::Pixel, registry, manager, key, pixelResult)) {
                    return {};
                }
                passDesc.mShaders.mPixel = key;
                hasPixelResult           = true;
            }

            if (pass.mPreset.IsEmptyString() && pass.mHasCompute) {
                RenderCore::FShaderRegistry::FShaderKey key{};
                ShaderCompiler::FShaderCompileResult    computeResult{};
                if (!CompileShaderFromAsset(pass.mCompute.mAsset, pass.mCompute.mEntry.ToView(),
                        Shader::EShaderStage::Compute, registry, manager, key, computeResult)) {
                    return {};
                }
                passDesc.mShaders.mCompute = key;
            }

            const Shader::FShaderReflection* vertexReflection =
                hasVertexResult ? &vertexResult.mReflection : nullptr;
            const Shader::FShaderReflection* pixelReflection =
                hasPixelResult ? &pixelResult.mReflection : nullptr;
            passDesc.mLayout = BuildMaterialLayout(vertexReflection, pixelReflection);
            LogMaterialLayout(passDesc.mLayout,
                SelectMaterialCBuffer(vertexReflection, pixelReflection), pass.mName);

            auto* rasterSourceAsset = static_cast<Asset::FShaderAsset*>(nullptr);
            if (pass.mHasPixel) {
                auto assetRef = manager.Load(pass.mPixel.mAsset);
                rasterSourceAsset =
                    assetRef ? static_cast<Asset::FShaderAsset*>(assetRef.Get()) : nullptr;
            }
            if (rasterSourceAsset == nullptr && pass.mHasVertex) {
                auto assetRef = manager.Load(pass.mVertex.mAsset);
                rasterSourceAsset =
                    assetRef ? static_cast<Asset::FShaderAsset*>(assetRef.Get()) : nullptr;
            }
            if (rasterSourceAsset != nullptr) {
                hasRasterState = TryParseRasterState(*rasterSourceAsset, rasterState);
            }

            if (passType == RenderCore::EMaterialPass::BasePass
                || passType == RenderCore::EMaterialPass::DepthPass
                || passType == RenderCore::EMaterialPass::ShadowPass) {
                passDesc.mState.mDepth.mDepthEnable  = true;
                passDesc.mState.mDepth.mDepthWrite   = true;
                passDesc.mState.mDepth.mDepthCompare = Rhi::ERhiCompareOp::GreaterEqual;
            }

            if (hasRasterState) {
                passDesc.mState.ApplyRasterState(rasterState);
            }

            if (pass.mRasterOverrides.HasAny()) {
                ApplyRasterOverrides(pass.mRasterOverrides, passDesc.mState.mRaster);
            }

            templ->SetPassDesc(passType, Move(passDesc));

            const auto overrideBlock = BuildTemplateOverrides(pass.mOverrides);
            if (HasTemplateOverrides(overrideBlock)) {
                templ->SetPassOverrides(passType, overrideBlock);
            }
        }
        if (templ->GetPasses().IsEmpty()) {
            return {};
        }
        return templ;
    }

    auto BuildRenderMaterialFromAsset(const Asset::FAssetHandle& handle,
        const Asset::FMeshMaterialParameterBlock& parameters, Asset::FAssetRegistry& registry,
        Asset::FAssetManager& manager) -> RenderCore::FMaterial {
        RenderCore::FMaterial material;
        if (!handle.IsValid() || handle.mType != Asset::EAssetType::MaterialTemplate) {
            return material;
        }

        auto  assetRef = manager.Load(handle);
        auto* materialAsset =
            assetRef ? static_cast<Asset::FMaterialAsset*>(assetRef.Get()) : nullptr;
        if (materialAsset == nullptr) {
            LogError(TEXT("Failed to load material template asset."));
            return material;
        }

        auto templ = BuildMaterialTemplateFromAsset(*materialAsset, registry, manager);
        if (!templ) {
            LogError(TEXT("Failed to build material template from asset."));
            return material;
        }

        material.SetTemplate(templ);

        auto schema       = Container::MakeShared<RenderCore::FMaterialSchema>();
        auto ensureSchema = [&](RenderCore::FMaterialParamId   id,
                                RenderCore::EMaterialParamType type) {
            if (id == 0U) {
                return;
            }
            if (schema->Find(id) != nullptr) {
                return;
            }
            switch (type) {
                case RenderCore::EMaterialParamType::Scalar:
                    schema->AddScalar(id);
                    break;
                case RenderCore::EMaterialParamType::Vector:
                    schema->AddVector(id);
                    break;
                case RenderCore::EMaterialParamType::Matrix:
                    schema->AddMatrix(id);
                    break;
                case RenderCore::EMaterialParamType::Texture:
                    schema->AddTexture(id);
                    break;
            }
        };

        for (const auto& param : parameters.GetScalars()) {
            ensureSchema(param.mNameHash, RenderCore::EMaterialParamType::Scalar);
        }
        for (const auto& param : parameters.GetVectors()) {
            ensureSchema(param.mNameHash, RenderCore::EMaterialParamType::Vector);
        }
        for (const auto& param : parameters.GetMatrices()) {
            ensureSchema(param.mNameHash, RenderCore::EMaterialParamType::Matrix);
        }
        for (const auto& param : parameters.GetTextures()) {
            ensureSchema(param.mNameHash, RenderCore::EMaterialParamType::Texture);
        }
        material.SetSchema(schema);

        for (const auto& param : parameters.GetScalars()) {
            material.SetScalar(param.mNameHash, param.mValue);
        }
        for (const auto& param : parameters.GetVectors()) {
            material.SetVector(param.mNameHash, param.mValue);
        }
        for (const auto& param : parameters.GetMatrices()) {
            material.SetMatrix(param.mNameHash, param.mValue);
        }
        for (const auto& param : parameters.GetTextures()) {
            Rhi::FRhiShaderResourceViewRef srv{};
            if (param.mTexture.IsValid()
                && param.mType == Asset::EMeshMaterialTextureType::Texture2D) {
                auto  textureAssetRef = manager.Load(param.mTexture);
                auto* textureAsset    = textureAssetRef
                       ? static_cast<Asset::FTexture2DAsset*>(textureAssetRef.Get())
                       : nullptr;
                if (textureAsset != nullptr) {
                    srv = CreateTextureSrv(param.mTexture, *textureAsset);
                }
            }

            Rhi::FRhiSamplerDesc samplerDesc{};
            samplerDesc.mDebugName.Assign(TEXT("MeshMaterialSampler"));
            auto sampler = Rhi::RHICreateSampler(samplerDesc);
            material.SetTexture(param.mNameHash, Move(srv), Move(sampler), param.mSamplerFlags);
        }

        auto applyOverrideTexture = [&](const Asset::FMeshMaterialTextureParam& param) {
            if (material.GetParameters().FindTextureParam(param.mNameHash) != nullptr) {
                return;
            }
            Rhi::FRhiShaderResourceViewRef srv{};
            if (param.mTexture.IsValid()
                && param.mType == Asset::EMeshMaterialTextureType::Texture2D) {
                auto  textureAssetRef = manager.Load(param.mTexture);
                auto* textureAsset    = textureAssetRef
                       ? static_cast<Asset::FTexture2DAsset*>(textureAssetRef.Get())
                       : nullptr;
                if (textureAsset != nullptr) {
                    srv = CreateTextureSrv(param.mTexture, *textureAsset);
                }
            }

            Rhi::FRhiSamplerDesc samplerDesc{};
            samplerDesc.mDebugName.Assign(TEXT("MeshMaterialSampler"));
            auto sampler = Rhi::RHICreateSampler(samplerDesc);
            material.SetTexture(param.mNameHash, Move(srv), Move(sampler), param.mSamplerFlags);
        };

        auto applyOverrides = [&](const Asset::FMeshMaterialParameterBlock& overrides) {
            for (const auto& param : overrides.GetScalars()) {
                if (material.GetParameters().FindScalarParam(param.mNameHash) != nullptr) {
                    continue;
                }
                ensureSchema(param.mNameHash, RenderCore::EMaterialParamType::Scalar);
                material.SetScalar(param.mNameHash, param.mValue);
            }
            for (const auto& param : overrides.GetVectors()) {
                if (material.GetParameters().FindVectorParam(param.mNameHash) != nullptr) {
                    continue;
                }
                ensureSchema(param.mNameHash, RenderCore::EMaterialParamType::Vector);
                material.SetVector(param.mNameHash, param.mValue);
            }
            for (const auto& param : overrides.GetMatrices()) {
                if (material.GetParameters().FindMatrixParam(param.mNameHash) != nullptr) {
                    continue;
                }
                ensureSchema(param.mNameHash, RenderCore::EMaterialParamType::Matrix);
                material.SetMatrix(param.mNameHash, param.mValue);
            }
            for (const auto& param : overrides.GetTextures()) {
                if (material.GetParameters().FindTextureParam(param.mNameHash) != nullptr) {
                    continue;
                }
                ensureSchema(param.mNameHash, RenderCore::EMaterialParamType::Texture);
                applyOverrideTexture(param);
            }
        };

        for (const auto& pass : materialAsset->GetPasses()) {
            applyOverrides(pass.mOverrides);
        }

        return material;
    }

    void ShutdownMaterialTextureSrvCache() noexcept { ClearTextureSrvCache(); }
} // namespace AltinaEngine::Rendering
