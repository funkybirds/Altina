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
#include "Types/Traits.h"

#include <algorithm>

using AltinaEngine::Move;
namespace AltinaEngine::RenderCore {
    namespace {
        using Core::Platform::Generic::Memcpy;
        using Core::Platform::Generic::Memset;

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
    } // namespace

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

        TextureNameHashes.Clear();
        TextureBindings.Clear();
        SamplerBindings.Clear();
        TextureNameHashes.Reserve(entries.Size());
        TextureBindings.Reserve(entries.Size());
        SamplerBindings.Reserve(entries.Size());

        for (const auto& entry : entries) {
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

            Rhi::FRhiBindGroupDesc groupDesc{};
            groupDesc.mLayout = layoutRef.Get();

            if (layout.PropertyBag.IsValid() && mCBuffer) {
                Rhi::FRhiBindGroupEntry cbufferEntry{};
                cbufferEntry.mBinding = layout.PropertyBag.GetBinding();
                cbufferEntry.mType    = Rhi::ERhiBindingType::ConstantBuffer;
                cbufferEntry.mBuffer  = mCBuffer.Get();
                cbufferEntry.mOffset  = 0ULL;
                cbufferEntry.mSize    = static_cast<u64>(layout.PropertyBag.GetSizeBytes());
                groupDesc.mEntries.PushBack(cbufferEntry);
            }

            const usize texCount = layout.TextureBindings.Size();
            for (usize i = 0U; i < texCount; ++i) {
                const auto nameHash =
                    (i < layout.TextureNameHashes.Size()) ? layout.TextureNameHashes[i] : 0U;
                const auto* param =
                    (nameHash != 0U) ? mParameters.FindTextureParam(nameHash) : nullptr;

                Rhi::FRhiBindGroupEntry texEntry{};
                texEntry.mBinding = layout.TextureBindings[i];
                texEntry.mType    = Rhi::ERhiBindingType::SampledTexture;
                texEntry.mTexture = (param && param->SRV) ? param->SRV->GetTexture() : nullptr;
                groupDesc.mEntries.PushBack(texEntry);

                if (i < layout.SamplerBindings.Size()) {
                    const auto samplerBinding = layout.SamplerBindings[i];
                    if (samplerBinding != kMaterialInvalidBinding) {
                        Rhi::FRhiBindGroupEntry samplerEntry{};
                        samplerEntry.mBinding = samplerBinding;
                        samplerEntry.mType    = Rhi::ERhiBindingType::Sampler;
                        samplerEntry.mSampler = param ? param->Sampler.Get() : nullptr;
                        groupDesc.mEntries.PushBack(samplerEntry);
                    }
                }
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
