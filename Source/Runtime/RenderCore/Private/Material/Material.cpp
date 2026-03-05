#include "Material/Material.h"
#include "Material/MaterialShaderMap.h"
#include "Material/MaterialTemplate.h"

#include "Platform/Generic/GenericPlatformDecl.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiResourceView.h"
#include "Rhi/RhiSampler.h"
#include "Shader/ShaderBindingUtility.h"
#include "Logging/Log.h"
#include "Types/Traits.h"
#include "Utility/Assert.h"

#include <algorithm>

using AltinaEngine::Move;
namespace AltinaEngine::RenderCore {
    namespace {
        using Core::Platform::Generic::Memcpy;
        using Core::Platform::Generic::Memset;
        using Core::Utility::DebugAssert;

        constexpr u32 kFnvOffset32 = 2166136261u;
        constexpr u32 kFnvPrime32  = 16777619u;

        auto BuildLayoutHash(const TVector<Rhi::FRhiBindGroupLayoutEntry>& entries, u32 setIndex)
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

        auto LayoutHasBindings(const FMaterialLayout& layout) -> bool {
            return layout.PropertyBag.IsValid() || !layout.TextureBindings.IsEmpty()
                || !layout.SamplerBindings.IsEmpty();
        }

        auto ToRhiFillMode(Shader::EShaderRasterFillMode mode) noexcept -> Rhi::ERhiRasterFillMode {
            switch (mode) {
                case Shader::EShaderRasterFillMode::Wireframe:
                    return Rhi::ERhiRasterFillMode::Wireframe;
                case Shader::EShaderRasterFillMode::Solid:
                default:
                    return Rhi::ERhiRasterFillMode::Solid;
            }
        }

        auto ToRhiCullMode(Shader::EShaderRasterCullMode mode) noexcept -> Rhi::ERhiRasterCullMode {
            switch (mode) {
                case Shader::EShaderRasterCullMode::None:
                    return Rhi::ERhiRasterCullMode::None;
                case Shader::EShaderRasterCullMode::Front:
                    return Rhi::ERhiRasterCullMode::Front;
                case Shader::EShaderRasterCullMode::Back:
                default:
                    return Rhi::ERhiRasterCullMode::Back;
            }
        }

        auto ToRhiFrontFace(Shader::EShaderRasterFrontFace mode) noexcept
            -> Rhi::ERhiRasterFrontFace {
            switch (mode) {
                case Shader::EShaderRasterFrontFace::CW:
                    return Rhi::ERhiRasterFrontFace::CW;
                case Shader::EShaderRasterFrontFace::CCW:
                default:
                    return Rhi::ERhiRasterFrontFace::CCW;
            }
        }

        struct FDefaultMaterialFallbacks {
            bool                bReady = false;
            Rhi::FRhiSamplerRef Sampler;
            Rhi::FRhiTextureRef WhiteTex;      // (1,1,1,1)
            Rhi::FRhiTextureRef BlackTex;      // (0,0,0,1)
            Rhi::FRhiTextureRef NormalFlatTex; // (0.5,0.5,1,1)
            Rhi::FRhiTextureRef GrayTex;       // (0.6,0.6,0.6,1)
        };

        auto GetDefaultMaterialFallbacksState() -> FDefaultMaterialFallbacks& {
            static FDefaultMaterialFallbacks s;
            return s;
        }

        auto EnsureDefaultMaterialFallbacks() -> FDefaultMaterialFallbacks& {
            auto& s = GetDefaultMaterialFallbacksState();
            if (s.bReady) {
                return s;
            }

            auto* device = Rhi::RHIGetDevice();
            if (!device) {
                return s;
            }

            // Default sampler (linear). Exact filter/wrap are backend defaults in this engine.
            if (!s.Sampler) {
                Rhi::FRhiSamplerDesc desc{};
                desc.mDebugName.Assign(TEXT("DefaultMaterial.Sampler"));
                s.Sampler = Rhi::RHICreateSampler(desc);
            }
            // Require refactor
            auto create1x1 = [&](const TChar* debugName, u8 r, u8 g, u8 b,
                                 u8 a) -> Rhi::FRhiTextureRef {
                Rhi::FRhiTextureDesc desc{};
                desc.mDebugName.Assign(debugName);
                desc.mWidth       = 1U;
                desc.mHeight      = 1U;
                desc.mMipLevels   = 1U;
                desc.mArrayLayers = 1U;
                desc.mSampleCount = 1U;
                desc.mFormat      = Rhi::ERhiFormat::R8G8B8A8Unorm;
                desc.mUsage       = Rhi::ERhiResourceUsage::Default;
                desc.mBindFlags   = Rhi::ERhiTextureBindFlags::ShaderResource;
                auto tex          = Rhi::RHICreateTexture(desc);
                if (tex) {
                    const u8 pixel[4] = { r, g, b, a };
                    device->UpdateTextureSubresource(
                        tex.Get(), Rhi::FRhiTextureSubresource{}, pixel, 4U, 4U);
                }
                return tex;
            };

            if (!s.WhiteTex) {
                s.WhiteTex = create1x1(TEXT("DefaultMaterial.White"), 255, 255, 255, 255);
            }
            if (!s.BlackTex) {
                s.BlackTex = create1x1(TEXT("DefaultMaterial.Black"), 0, 0, 0, 255);
            }
            if (!s.NormalFlatTex) {
                // Tangent-space flat normal encoded in [0,1]: (0.5, 0.5, 1.0).
                s.NormalFlatTex = create1x1(TEXT("DefaultMaterial.NormalFlat"), 128, 128, 255, 255);
            }
            if (!s.GrayTex) {
                // 0.6 * 255 ~= 153.
                s.GrayTex = create1x1(TEXT("DefaultMaterial.Gray"), 153, 153, 153, 255);
            }

            s.bReady = true;
            return s;
        }
    } // namespace

    void ShutdownMaterialFallbacks() noexcept {
        auto& s = GetDefaultMaterialFallbacksState();
        s.WhiteTex.Reset();
        s.BlackTex.Reset();
        s.NormalFlatTex.Reset();
        s.GrayTex.Reset();
        s.Sampler.Reset();
        s.bReady = false;
    }

    auto HashMaterialParamName(FStringView name) noexcept -> FMaterialParamId {
        if (name.IsEmpty()) {
            return 0U;
        }

        u32         hash   = kFnvOffset32;
        const auto* data   = name.Data();
        const auto  length = name.Length();
        for (usize i = 0U; i < length; ++i) {
            hash ^= static_cast<u32>(static_cast<FStringView::TUnsigned>(data[i]));
            hash *= kFnvPrime32;
        }
        return hash;
    }

    auto HashMaterialParamName(const TChar* name) noexcept -> FMaterialParamId {
        if (name == nullptr || name[0] == static_cast<TChar>(0)) {
            return 0U;
        }
        return HashMaterialParamName(FStringView(name));
    }

    void FMaterialLayout::Reset() {
        PropertyBag.Reset();
        TextureNameHashes.Clear();
        TextureBindings.Clear();
        SamplerBindings.Clear();
        PropertyMap.clear();
    }

    void FMaterialLayout::InitFromConstantBuffer(const Shader::FShaderConstantBuffer& cbuffer) {
        PropertyBag.Init(cbuffer);
        PropertyMap.clear();
        for (const auto& member : cbuffer.mMembers) {
            Shader::FShaderPropertyBag::FPropertyDesc desc{};
            desc.mOffset        = member.mOffset;
            desc.mSize          = member.mSize;
            desc.mElementCount  = member.mElementCount;
            desc.mElementStride = member.mElementStride;
            const auto hash     = HashMaterialParamName(member.mName.ToView());
            if (hash != 0U) {
                PropertyMap[hash] = desc;
            }
        }
    }

    void FMaterialLayout::AddTextureBinding(
        FMaterialParamId nameHash, u32 textureBinding, u32 samplerBinding) {
        TextureNameHashes.PushBack(nameHash);
        TextureBindings.PushBack(textureBinding);
        SamplerBindings.PushBack(samplerBinding);
    }

    void FMaterialLayout::SortTextureBindings() {
        const usize count = TextureBindings.Size();
        if (count == 0U) {
            return;
        }

        struct FEntry {
            FMaterialParamId mNameHash       = 0U;
            u32              mTextureBinding = 0U;
            u32              mSamplerBinding = kMaterialInvalidBinding;
        };

        TVector<FEntry> entries;
        entries.Reserve(count);

        const usize samplerCount = SamplerBindings.Size();
        const usize nameCount    = TextureNameHashes.Size();
        const usize safeCount    = (nameCount < count) ? nameCount : count;

        for (usize i = 0U; i < safeCount; ++i) {
            FEntry entry{};
            entry.mNameHash       = TextureNameHashes[i];
            entry.mTextureBinding = TextureBindings[i];
            entry.mSamplerBinding =
                (i < samplerCount) ? SamplerBindings[i] : kMaterialInvalidBinding;
            entries.PushBack(entry);
        }

        std::sort(entries.begin(), entries.end(),
            [](const FEntry& a, const FEntry& b) { return a.mTextureBinding < b.mTextureBinding; });

        TVector<FEntry> deduplicated;
        deduplicated.Reserve(entries.Size());
        for (const auto& entry : entries) {
            if (deduplicated.IsEmpty()) {
                deduplicated.PushBack(entry);
                continue;
            }

            auto& last = deduplicated.Back();
            if (last.mTextureBinding != entry.mTextureBinding) {
                deduplicated.PushBack(entry);
                continue;
            }

            // Merge duplicate entries emitted by cross-stage reflection for the same resource.
            // Some shader pipelines may alias names to the same register; keep running and log.
            if (last.mNameHash != entry.mNameHash) {
                LogWarningCat(TEXT("RenderCore.Material"),
                    "Material layout has conflicting texture names on same binding={} "
                    "(nameHashA=0x{:08X}, nameHashB=0x{:08X}); keeping first.",
                    entry.mTextureBinding, static_cast<u32>(last.mNameHash),
                    static_cast<u32>(entry.mNameHash));
            }

            if (last.mSamplerBinding == kMaterialInvalidBinding) {
                last.mSamplerBinding = entry.mSamplerBinding;
            } else if (entry.mSamplerBinding != kMaterialInvalidBinding) {
                if (last.mSamplerBinding != entry.mSamplerBinding) {
                    LogWarningCat(TEXT("RenderCore.Material"),
                        "Material layout has conflicting sampler bindings for texture binding={} "
                        "(samplerA={}, samplerB={}); keeping first.",
                        entry.mTextureBinding, last.mSamplerBinding, entry.mSamplerBinding);
                }
            }
        }

        TextureNameHashes.Clear();
        TextureBindings.Clear();
        SamplerBindings.Clear();
        TextureNameHashes.Reserve(deduplicated.Size());
        TextureBindings.Reserve(deduplicated.Size());
        SamplerBindings.Reserve(deduplicated.Size());

        for (const auto& entry : deduplicated) {
            TextureNameHashes.PushBack(entry.mNameHash);
            TextureBindings.PushBack(entry.mTextureBinding);
            SamplerBindings.PushBack(entry.mSamplerBinding);
        }
    }

    auto FMaterialLayout::FindProperty(FMaterialParamId id) const noexcept
        -> const Shader::FShaderPropertyBag::FPropertyDesc* {
        const auto it = PropertyMap.find(id);
        if (it == PropertyMap.end()) {
            return nullptr;
        }
        return &it->second;
    }

    void FMaterialPassState::ApplyRasterState(const Shader::FShaderRasterState& state) noexcept {
        Raster.mFillMode             = ToRhiFillMode(state.mFillMode);
        Raster.mCullMode             = ToRhiCullMode(state.mCullMode);
        Raster.mFrontFace            = ToRhiFrontFace(state.mFrontFace);
        Raster.mDepthBias            = state.mDepthBias;
        Raster.mDepthBiasClamp       = state.mDepthBiasClamp;
        Raster.mSlopeScaledDepthBias = state.mSlopeScaledDepthBias;
        Raster.mDepthClip            = state.mDepthClip;
        Raster.mConservativeRaster   = state.mConservativeRaster;
    }

    auto FMaterialShaderMap::Find(EMaterialPass pass) const noexcept
        -> const FMaterialPassShaders* {
        const auto it = PassShaders.find(pass);
        if (it == PassShaders.end()) {
            return nullptr;
        }
        return &it->second;
    }

    auto FMaterialShaderMap::Has(EMaterialPass pass) const noexcept -> bool {
        return PassShaders.find(pass) != PassShaders.end();
    }

    void FMaterialTemplate::SetPassDesc(EMaterialPass pass, FMaterialPassDesc desc) {
        mPasses[pass] = Move(desc);
    }

    void FMaterialTemplate::SetPassOverrides(
        EMaterialPass pass, FMaterialParameterBlock overrides) {
        mOverrides[pass] = Move(overrides);
    }

    auto FMaterialTemplate::FindPassDesc(EMaterialPass pass) const noexcept
        -> const FMaterialPassDesc* {
        const auto it = mPasses.find(pass);
        if (it == mPasses.end()) {
            return nullptr;
        }
        return &it->second;
    }

    auto FMaterialTemplate::FindLayout(EMaterialPass pass) const noexcept
        -> const FMaterialLayout* {
        const auto* desc = FindPassDesc(pass);
        return desc ? &desc->Layout : nullptr;
    }

    auto FMaterialTemplate::FindShaders(EMaterialPass pass) const noexcept
        -> const FMaterialPassShaders* {
        const auto* desc = FindPassDesc(pass);
        return desc ? &desc->Shaders : nullptr;
    }

    auto FMaterialTemplate::FindState(EMaterialPass pass) const noexcept
        -> const FMaterialPassState* {
        const auto* desc = FindPassDesc(pass);
        return desc ? &desc->State : nullptr;
    }

    auto FMaterialTemplate::FindOverrides(EMaterialPass pass) const noexcept
        -> const FMaterialParameterBlock* {
        const auto it = mOverrides.find(pass);
        if (it == mOverrides.end()) {
            return nullptr;
        }
        return &it->second;
    }

    auto FMaterialTemplate::FindAnyPassDesc() const noexcept -> const FMaterialPassDesc* {
        if (mPasses.empty()) {
            return nullptr;
        }
        return &mPasses.begin()->second;
    }

    void FMaterial::SetTemplate(TShared<FMaterialTemplate> templ) noexcept {
        mTemplate      = Move(templ);
        mDirtyCBuffer  = true;
        mDirtyBindings = true;
        if (IsInitialized()) {
            UpdateResource();
        }
    }

    void FMaterial::SetSchema(TShared<FMaterialSchema> schema) noexcept { mSchema = Move(schema); }

    auto FMaterial::SetScalar(FMaterialParamId id, f32 value) -> bool {
        if (id == 0U || !IsSchemaTypeMatch(id, EMaterialParamType::Scalar)) {
            return false;
        }

        const bool changed = mParameters.SetScalar(id, value);
        if (changed) {
            mDirtyCBuffer = true;
        }
        if (IsInitialized()) {
            UpdateResource();
        }
        return changed;
    }

    auto FMaterial::SetVector(FMaterialParamId id, const Math::FVector4f& value) -> bool {
        if (id == 0U || !IsSchemaTypeMatch(id, EMaterialParamType::Vector)) {
            return false;
        }

        const bool changed = mParameters.SetVector(id, value);
        if (changed) {
            mDirtyCBuffer = true;
        }
        if (IsInitialized()) {
            UpdateResource();
        }
        return changed;
    }

    auto FMaterial::SetMatrix(FMaterialParamId id, const Math::FMatrix4x4f& value) -> bool {
        if (id == 0U || !IsSchemaTypeMatch(id, EMaterialParamType::Matrix)) {
            return false;
        }

        const bool changed = mParameters.SetMatrix(id, value);
        if (changed) {
            mDirtyCBuffer = true;
        }
        if (IsInitialized()) {
            UpdateResource();
        }
        return changed;
    }

    auto FMaterial::SetTexture(FMaterialParamId id, Rhi::FRhiShaderResourceViewRef srv,
        Rhi::FRhiSamplerRef sampler, u32 samplerFlags) -> bool {
        if (id == 0U || !IsSchemaTypeMatch(id, EMaterialParamType::Texture)) {
            return false;
        }

        const bool changed = mParameters.SetTexture(id, Move(srv), Move(sampler), samplerFlags);
        if (changed) {
            mDirtyBindings = true;
        }
        if (IsInitialized()) {
            UpdateResource();
        }
        return changed;
    }

    auto FMaterial::FindPassDesc(EMaterialPass pass) const noexcept -> const FMaterialPassDesc* {
        return mTemplate ? mTemplate->FindPassDesc(pass) : nullptr;
    }

    auto FMaterial::FindShaders(EMaterialPass pass) const noexcept -> const FMaterialPassShaders* {
        return mTemplate ? mTemplate->FindShaders(pass) : nullptr;
    }

    auto FMaterial::FindState(EMaterialPass pass) const noexcept -> const FMaterialPassState* {
        return mTemplate ? mTemplate->FindState(pass) : nullptr;
    }

    auto FMaterial::FindLayout(EMaterialPass pass) const noexcept -> const FMaterialLayout* {
        return mTemplate ? mTemplate->FindLayout(pass) : nullptr;
    }

    auto FMaterial::GetBindGroup(EMaterialPass pass) const noexcept -> Rhi::FRhiBindGroupRef {
        const auto it = mBindGroups.find(pass);
        if (it == mBindGroups.end()) {
            return {};
        }
        return it->second;
    }

    void FMaterial::InitRHI() {
        mDirtyCBuffer  = true;
        mDirtyBindings = true;
        UpdateRHI();
    }

    void FMaterial::ReleaseRHI() {
        mBindGroups.clear();
        mBindGroupLayouts.clear();
        mCBuffer.Reset();
        mCBufferData.Clear();
    }

    void FMaterial::UpdateRHI() {
        if (!mTemplate) {
            mBindGroups.clear();
            mBindGroupLayouts.clear();
            mCBuffer.Reset();
            mCBufferData.Clear();
            mDirtyCBuffer  = false;
            mDirtyBindings = false;
            return;
        }

        const auto* baseDesc = mTemplate->FindPassDesc(EMaterialPass::BasePass);
        const auto* anyDesc  = baseDesc ? baseDesc : mTemplate->FindAnyPassDesc();
        if (!anyDesc) {
            mDirtyCBuffer  = false;
            mDirtyBindings = false;
            return;
        }

        const auto& defaultLayout = anyDesc->Layout;
        bool        recreated     = false;
        bool        uploaded      = false;

        if (!defaultLayout.PropertyBag.IsValid()) {
            mCBuffer.Reset();
            mCBufferData.Clear();
            mDirtyCBuffer = false;
        } else if (mDirtyCBuffer || !mCBuffer) {
            UpdateCBuffer(defaultLayout, recreated, uploaded);
        }

        if (mDirtyBindings || recreated) {
            UpdateBindGroups(*mTemplate, defaultLayout);
        }

        if (uploaded) {
            mDirtyCBuffer = false;
        }
        mDirtyBindings = false;
    }

    auto FMaterial::IsSchemaTypeMatch(FMaterialParamId id, EMaterialParamType type) const noexcept
        -> bool {
        if (!mSchema) {
            return true;
        }
        const auto* desc = mSchema->Find(id);
        if (!desc) {
            return false;
        }
        return desc->Type == type;
    }

    void FMaterial::UpdateCBuffer(
        const FMaterialLayout& layout, bool& outRecreated, bool& outUploaded) {
        outRecreated = false;
        outUploaded  = false;

        const u32 sizeBytes = layout.PropertyBag.GetSizeBytes();
        if (sizeBytes == 0U) {
            mCBuffer.Reset();
            mCBufferData.Clear();
            return;
        }

        if (!mCBuffer || mCBuffer->GetDesc().mSizeBytes != sizeBytes) {
            Rhi::FRhiBufferDesc desc{};
            desc.mDebugName = Container::FString(TEXT("MaterialCBuffer"));
            desc.mSizeBytes = sizeBytes;
            desc.mUsage     = Rhi::ERhiResourceUsage::Dynamic;
            desc.mBindFlags = Rhi::ERhiBufferBindFlags::Constant;
            desc.mCpuAccess = Rhi::ERhiCpuAccess::Write;
            mCBuffer        = Rhi::RHICreateBuffer(desc);
            outRecreated    = true;
        }

        if (!mCBuffer) {
            return;
        }

        if (mCBufferData.Size() != sizeBytes) {
            mCBufferData.Resize(sizeBytes);
        }
        if (!mCBufferData.IsEmpty()) {
            Memset(mCBufferData.Data(), 0, static_cast<usize>(mCBufferData.Size()));
        }

        // Reasonable defaults for common PBR parameters so "albedo-only" materials are still
        // visible without requiring every asset to author metallic/roughness/occlusion/emissive.
        auto writeDefault = [&](const TChar* name, const void* value, u32 valueSize) {
            const auto  id   = HashMaterialParamName(name);
            const auto* prop = layout.FindProperty(id);
            if (!prop) {
                return;
            }
            if (prop->mSize == 0U || prop->mOffset + prop->mSize > mCBufferData.Size()) {
                return;
            }
            const u32 copySize = (valueSize < prop->mSize) ? valueSize : prop->mSize;
            Memcpy(mCBufferData.Data() + prop->mOffset, value, copySize);
        };

        {
            const Math::FVector4f baseColor(1.0f, 1.0f, 1.0f, 1.0f);
            const f32             metallic  = 0.0f;
            const f32             roughness = 0.6f;
            const f32             occlusion = 1.0f;
            const Math::FVector4f emissive(0.0f, 0.0f, 0.0f, 0.0f);
            const f32             emissiveIntensity = 0.0f;
            const f32             normalMapStrength = 1.0f;

            writeDefault(
                TEXT("BaseColor"), &baseColor.mComponents[0], sizeof(baseColor.mComponents));
            writeDefault(TEXT("Metallic"), &metallic, sizeof(metallic));
            writeDefault(TEXT("Roughness"), &roughness, sizeof(roughness));
            writeDefault(TEXT("Occlusion"), &occlusion, sizeof(occlusion));
            // cbuffer member is float3, but may be padded; writing 16 bytes is harmless within
            // prop->mSize.
            writeDefault(TEXT("Emissive"), &emissive.mComponents[0], sizeof(emissive.mComponents));
            writeDefault(TEXT("EmissiveIntensity"), &emissiveIntensity, sizeof(emissiveIntensity));
            writeDefault(TEXT("NormalMapStrength"), &normalMapStrength, sizeof(normalMapStrength));
        }

        auto applyParam = [&](FMaterialParamId id, const void* data, u32 size) {
            const auto* prop = layout.FindProperty(id);
            if (!prop || data == nullptr) {
                return;
            }
            if (prop->mOffset >= mCBufferData.Size()) {
                return;
            }
            u32 copySize = size;
            if (prop->mSize != 0U && prop->mSize < copySize) {
                copySize = prop->mSize;
            }
            if (prop->mOffset + copySize > mCBufferData.Size()) {
                return;
            }
            Memcpy(mCBufferData.Data() + prop->mOffset, data, copySize);
        };

        for (const auto& scalar : mParameters.GetScalars()) {
            applyParam(scalar.NameHash, &scalar.Value, sizeof(scalar.Value));
        }
        for (const auto& vector : mParameters.GetVectors()) {
            applyParam(vector.NameHash, vector.Value.mComponents, sizeof(vector.Value));
        }
        for (const auto& matrix : mParameters.GetMatrices()) {
            applyParam(matrix.NameHash, &matrix.Value.mElements[0][0], sizeof(matrix.Value));
        }

        auto lock = mCBuffer->Lock(
            0ULL, static_cast<u64>(mCBufferData.Size()), Rhi::ERhiBufferLockMode::WriteDiscard);
        if (!lock.IsValid()) {
            return;
        }
        Memcpy(lock.mData, mCBufferData.Data(), static_cast<usize>(mCBufferData.Size()));
        mCBuffer->Unlock(lock);
        outUploaded = true;
    }

    void FMaterial::UpdateBindGroups(
        const FMaterialTemplate& templ, const FMaterialLayout& defaultLayout) {
        auto* device = Rhi::RHIGetDevice();
        if (!device) {
            return;
        }

        mBindGroups.clear();
        mBindGroupLayouts.clear();

        for (const auto& entry : templ.GetPasses()) {
            const auto  pass       = entry.first;
            const auto& passDesc   = entry.second;
            const auto& passLayout = passDesc.Layout;
            const auto& layout     = LayoutHasBindings(passLayout) ? passLayout : defaultLayout;

            if (!LayoutHasBindings(layout)) {
                continue;
            }

            Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
            layoutDesc.mSetIndex = layout.PropertyBag.IsValid() ? layout.PropertyBag.GetSet() : 0U;

            TVector<Rhi::FRhiBindGroupLayoutEntry> layoutEntries;

            if (layout.PropertyBag.IsValid()) {
                Rhi::FRhiBindGroupLayoutEntry entryDesc{};
                entryDesc.mBinding          = layout.PropertyBag.GetBinding();
                entryDesc.mType             = Rhi::ERhiBindingType::ConstantBuffer;
                entryDesc.mVisibility       = Rhi::ERhiShaderStageFlags::All;
                entryDesc.mArrayCount       = 1U;
                entryDesc.mHasDynamicOffset = false;
                layoutEntries.PushBack(entryDesc);
            }

            const usize textureCount = layout.TextureBindings.Size();
            for (usize i = 0U; i < textureCount; ++i) {
                Rhi::FRhiBindGroupLayoutEntry entryDesc{};
                entryDesc.mBinding          = layout.TextureBindings[i];
                entryDesc.mType             = Rhi::ERhiBindingType::SampledTexture;
                entryDesc.mVisibility       = Rhi::ERhiShaderStageFlags::All;
                entryDesc.mArrayCount       = 1U;
                entryDesc.mHasDynamicOffset = false;
                layoutEntries.PushBack(entryDesc);
            }

            const usize samplerCount = layout.SamplerBindings.Size();
            for (usize i = 0U; i < samplerCount; ++i) {
                if (layout.SamplerBindings[i] == kMaterialInvalidBinding) {
                    continue;
                }
                Rhi::FRhiBindGroupLayoutEntry entryDesc{};
                entryDesc.mBinding          = layout.SamplerBindings[i];
                entryDesc.mType             = Rhi::ERhiBindingType::Sampler;
                entryDesc.mVisibility       = Rhi::ERhiShaderStageFlags::All;
                entryDesc.mArrayCount       = 1U;
                entryDesc.mHasDynamicOffset = false;
                layoutEntries.PushBack(entryDesc);
            }

            std::sort(
                layoutEntries.begin(), layoutEntries.end(), [](const auto& lhs, const auto& rhs) {
                    if (lhs.mBinding != rhs.mBinding) {
                        return lhs.mBinding < rhs.mBinding;
                    }
                    return lhs.mType < rhs.mType;
                });

            layoutDesc.mEntries    = layoutEntries;
            layoutDesc.mLayoutHash = BuildLayoutHash(layoutDesc.mEntries, layoutDesc.mSetIndex);

            auto layoutRef = device->CreateBindGroupLayout(layoutDesc);
            if (!layoutRef) {
                continue;
            }

            ShaderBinding::FBindGroupBuilder groupBuilder(layoutRef.Get());
            bool                             passValid = true;

            auto                             logPassLayoutContext = [&]() {
                LogErrorCat(TEXT("RenderCore.Material"),
                                                "Material pass {} layout context: set={}, hasCBuffer={}, texCount={}, samplerCount={}.",
                                                static_cast<u32>(pass), layoutDesc.mSetIndex,
                                                static_cast<u32>(layout.PropertyBag.IsValid()),
                                                static_cast<u32>(layout.TextureBindings.Size()),
                                                static_cast<u32>(layout.SamplerBindings.Size()));
            };

            if (layout.PropertyBag.IsValid() && mCBuffer) {
                if (!groupBuilder.AddBuffer(layout.PropertyBag.GetBinding(), mCBuffer.Get(), 0ULL,
                        static_cast<u64>(layout.PropertyBag.GetSizeBytes()))) {
                    LogErrorCat(TEXT("RenderCore.Material"),
                        "Material pass {}: failed to add cbuffer entry (binding={}); skipping pass.",
                        static_cast<u32>(pass), layout.PropertyBag.GetBinding());
                    logPassLayoutContext();
                    passValid = false;
                }
            }

            const usize texCount = layout.TextureBindings.Size();
            for (usize i = 0U; i < texCount; ++i) {
                if (!passValid) {
                    break;
                }
                const auto nameHash =
                    (i < layout.TextureNameHashes.Size()) ? layout.TextureNameHashes[i] : 0U;
                const auto* param =
                    (nameHash != 0U) ? mParameters.FindTextureParam(nameHash) : nullptr;
                LogInfo(TEXT("Material BindGroup pass={} texIndex={} nameHash=0x{:08X} binding={} ")
                            TEXT("samplerBinding={} hasParam={} srv={} sampler={}"),
                    static_cast<u32>(pass), static_cast<u32>(i), nameHash,
                    layout.TextureBindings[i],
                    (i < layout.SamplerBindings.Size()) ? layout.SamplerBindings[i]
                                                        : kMaterialInvalidBinding,
                    (param != nullptr) ? 1 : 0,
                    (param && param->SRV) ? static_cast<const void*>(param->SRV.Get()) : nullptr,
                    (param && param->Sampler) ? static_cast<const void*>(param->Sampler.Get())
                                              : nullptr);

                Rhi::FRhiTexture* texture =
                    (param && param->SRV) ? param->SRV->GetTexture() : nullptr;

                if (texture == nullptr) {
                    // Bind safe fallbacks for missing textures so the shader doesn't sample null
                    // SRVs (which returns 0 and can collapse lighting to black).
                    auto&      fb            = EnsureDefaultMaterialFallbacks();
                    const auto hBaseColor    = HashMaterialParamName(TEXT("BaseColorTex"));
                    const auto hNormal       = HashMaterialParamName(TEXT("NormalTex"));
                    const auto hMetallic     = HashMaterialParamName(TEXT("MetallicTex"));
                    const auto hRoughness    = HashMaterialParamName(TEXT("RoughnessTex"));
                    const auto hEmissive     = HashMaterialParamName(TEXT("EmissiveTex"));
                    const auto hOcclusion    = HashMaterialParamName(TEXT("OcclusionTex"));
                    const auto hSpecular     = HashMaterialParamName(TEXT("SpecularTex"));
                    const auto hDisplacement = HashMaterialParamName(TEXT("DisplacementTex"));

                    if (nameHash == hBaseColor) {
                        texture = fb.WhiteTex.Get();
                    } else if (nameHash == hNormal) {
                        texture = fb.NormalFlatTex.Get();
                    } else if (nameHash == hRoughness) {
                        texture = fb.GrayTex.Get();
                    } else if (nameHash == hOcclusion) {
                        texture = fb.WhiteTex.Get();
                    } else if (nameHash == hMetallic || nameHash == hEmissive
                        || nameHash == hSpecular || nameHash == hDisplacement) {
                        texture = fb.BlackTex.Get();
                    } else {
                        // Unknown texture slot: default to white to avoid multiplying to black.
                        texture = fb.WhiteTex.Get();
                    }
                }
                if (!groupBuilder.AddTexture(layout.TextureBindings[i], texture)) {
                    LogErrorCat(TEXT("RenderCore.Material"),
                        "Material pass {}: failed to add texture entry (binding={}, texIndex={}, nameHash=0x{:08X}); skipping pass.",
                        static_cast<u32>(pass), layout.TextureBindings[i], static_cast<u32>(i),
                        static_cast<u32>(nameHash));
                    logPassLayoutContext();
                    passValid = false;
                    break;
                }

                if (i < layout.SamplerBindings.Size()) {
                    const auto samplerBinding = layout.SamplerBindings[i];
                    if (samplerBinding != kMaterialInvalidBinding) {
                        Rhi::FRhiSampler* sampler = (param && param->Sampler)
                            ? param->Sampler.Get()
                            : EnsureDefaultMaterialFallbacks().Sampler.Get();
                        if (!groupBuilder.AddSampler(samplerBinding, sampler)) {
                            LogErrorCat(TEXT("RenderCore.Material"),
                                "Material pass {}: failed to add sampler entry (binding={}, texIndex={}, nameHash=0x{:08X}); skipping pass.",
                                static_cast<u32>(pass), samplerBinding, static_cast<u32>(i),
                                static_cast<u32>(nameHash));
                            logPassLayoutContext();
                            passValid = false;
                            break;
                        }
                    }
                }
            }
            if (!passValid) {
                continue;
            }

            Rhi::FRhiBindGroupDesc groupDesc{};
            if (!groupBuilder.Build(groupDesc)) {
                LogErrorCat(TEXT("RenderCore.Material"),
                    "Material pass {}: bind-group build failed (layout mismatch); skipping pass.",
                    static_cast<u32>(pass));
                logPassLayoutContext();
                continue;
            }
            auto groupRef = device->CreateBindGroup(groupDesc);
            if (!groupRef) {
                continue;
            }

            mBindGroupLayouts[pass] = layoutRef;
            mBindGroups[pass]       = groupRef;
        }
    }

} // namespace AltinaEngine::RenderCore
