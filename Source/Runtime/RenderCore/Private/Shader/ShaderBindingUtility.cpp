#include "Shader/ShaderBindingUtility.h"

#include "Algorithm/Sort.h"
#include "Algorithm/CStringUtils.h"
#include "Utility/String/StringViewUtility.h"

namespace AltinaEngine::RenderCore::ShaderBinding {
    namespace {
        [[nodiscard]] auto MakeLookupKey(u32 nameHash, Rhi::ERhiBindingType type) noexcept -> u64 {
            return (static_cast<u64>(nameHash) << 32U) | static_cast<u64>(static_cast<u8>(type));
        }

        [[nodiscard]] auto LayoutHasBinding(const Rhi::FRhiBindGroupLayout* layout, u32 binding,
            Rhi::ERhiBindingType type) -> bool {
            if (layout == nullptr) {
                return false;
            }
            for (const auto& entry : layout->GetDesc().mEntries) {
                if (entry.mBinding == binding && entry.mType == type) {
                    return true;
                }
            }
            return false;
        }

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
    } // namespace

    auto ToRhiShaderStageFlags(Shader::EShaderStage stage) noexcept -> Rhi::ERhiShaderStageFlags {
        using Rhi::ERhiShaderStageFlags;
        switch (stage) {
            case Shader::EShaderStage::Vertex:
                return ERhiShaderStageFlags::Vertex;
            case Shader::EShaderStage::Pixel:
                return ERhiShaderStageFlags::Pixel;
            case Shader::EShaderStage::Compute:
                return ERhiShaderStageFlags::Compute;
            case Shader::EShaderStage::Geometry:
                return ERhiShaderStageFlags::Geometry;
            case Shader::EShaderStage::Hull:
                return ERhiShaderStageFlags::Hull;
            case Shader::EShaderStage::Domain:
                return ERhiShaderStageFlags::Domain;
            case Shader::EShaderStage::Mesh:
                return ERhiShaderStageFlags::Mesh;
            case Shader::EShaderStage::Amplification:
                return ERhiShaderStageFlags::Amplification;
            case Shader::EShaderStage::Library:
            default:
                return ERhiShaderStageFlags::All;
        }
    }

    auto OrShaderStageFlags(Rhi::ERhiShaderStageFlags lhs, Rhi::ERhiShaderStageFlags rhs) noexcept
        -> Rhi::ERhiShaderStageFlags {
        return static_cast<Rhi::ERhiShaderStageFlags>(static_cast<u8>(lhs) | static_cast<u8>(rhs));
    }

    auto ToRhiBindingType(const Shader::FShaderResourceBinding& resource) noexcept
        -> Rhi::ERhiBindingType {
        using Rhi::ERhiBindingType;
        switch (resource.mType) {
            case Shader::EShaderResourceType::ConstantBuffer:
                return ERhiBindingType::ConstantBuffer;
            case Shader::EShaderResourceType::Texture:
                return ERhiBindingType::SampledTexture;
            case Shader::EShaderResourceType::Sampler:
                return ERhiBindingType::Sampler;
            case Shader::EShaderResourceType::StorageBuffer:
                return (resource.mAccess == Shader::EShaderResourceAccess::ReadWrite)
                    ? ERhiBindingType::StorageBuffer
                    : ERhiBindingType::SampledBuffer;
            case Shader::EShaderResourceType::StorageTexture:
                return (resource.mAccess == Shader::EShaderResourceAccess::ReadWrite)
                    ? ERhiBindingType::StorageTexture
                    : ERhiBindingType::SampledTexture;
            case Shader::EShaderResourceType::AccelerationStructure:
                return ERhiBindingType::AccelerationStructure;
            default:
                return ERhiBindingType::SampledTexture;
        }
    }

    auto BuildBindGroupLayoutFromShaderSet(const RenderCore::FShaderRegistry& registry,
        const TVector<RenderCore::FShaderRegistry::FShaderKey>& shaderKeys, u32 setIndex,
        Rhi::FRhiBindGroupLayoutDesc& outLayoutDesc) -> bool {
        outLayoutDesc           = {};
        outLayoutDesc.mSetIndex = setIndex;

        for (const auto& shaderKey : shaderKeys) {
            if (!shaderKey.IsValid()) {
                continue;
            }

            const auto shader = registry.FindShader(shaderKey);
            if (!shader) {
                continue;
            }

            const auto stageFlags = ToRhiShaderStageFlags(shader->GetDesc().mStage);
            for (const auto& resource : shader->GetDesc().mReflection.mResources) {
                if (resource.mSet != setIndex) {
                    continue;
                }

                const auto type     = ToRhiBindingType(resource);
                auto*      existing = static_cast<Rhi::FRhiBindGroupLayoutEntry*>(nullptr);
                for (auto& entry : outLayoutDesc.mEntries) {
                    if (entry.mBinding == resource.mBinding && entry.mType == type) {
                        existing = &entry;
                        break;
                    }
                }

                if (existing != nullptr) {
                    existing->mVisibility = OrShaderStageFlags(existing->mVisibility, stageFlags);
                    continue;
                }

                Rhi::FRhiBindGroupLayoutEntry entry{};
                entry.mBinding    = resource.mBinding;
                entry.mType       = type;
                entry.mVisibility = stageFlags;
                outLayoutDesc.mEntries.PushBack(entry);
            }
        }

        if (outLayoutDesc.mEntries.IsEmpty()) {
            return false;
        }

        Core::Algorithm::Sort(outLayoutDesc.mEntries.begin(), outLayoutDesc.mEntries.end(),
            [](const auto& lhs, const auto& rhs) {
                if (lhs.mBinding != rhs.mBinding) {
                    return lhs.mBinding < rhs.mBinding;
                }
                return lhs.mType < rhs.mType;
            });
        outLayoutDesc.mLayoutHash =
            BuildLayoutHash(outLayoutDesc.mEntries, outLayoutDesc.mSetIndex);
        return true;
    }

    auto BuildBindGroupLayoutFromShaders(const TVector<Rhi::FRhiShader*>& shaders, u32 setIndex,
        Rhi::FRhiBindGroupLayoutDesc& outLayoutDesc) -> bool {
        outLayoutDesc           = {};
        outLayoutDesc.mSetIndex = setIndex;

        for (const auto* shader : shaders) {
            if (shader == nullptr) {
                continue;
            }

            const auto stageFlags = ToRhiShaderStageFlags(shader->GetDesc().mStage);
            for (const auto& resource : shader->GetDesc().mReflection.mResources) {
                if (resource.mSet != setIndex) {
                    continue;
                }

                const auto type     = ToRhiBindingType(resource);
                auto*      existing = static_cast<Rhi::FRhiBindGroupLayoutEntry*>(nullptr);
                for (auto& entry : outLayoutDesc.mEntries) {
                    if (entry.mBinding == resource.mBinding && entry.mType == type) {
                        existing = &entry;
                        break;
                    }
                }
                if (existing != nullptr) {
                    existing->mVisibility = OrShaderStageFlags(existing->mVisibility, stageFlags);
                    continue;
                }

                Rhi::FRhiBindGroupLayoutEntry entry{};
                entry.mBinding    = resource.mBinding;
                entry.mType       = type;
                entry.mVisibility = stageFlags;
                outLayoutDesc.mEntries.PushBack(entry);
            }
        }

        if (outLayoutDesc.mEntries.IsEmpty()) {
            return false;
        }

        Core::Algorithm::Sort(outLayoutDesc.mEntries.begin(), outLayoutDesc.mEntries.end(),
            [](const auto& lhs, const auto& rhs) {
                if (lhs.mBinding != rhs.mBinding) {
                    return lhs.mBinding < rhs.mBinding;
                }
                return lhs.mType < rhs.mType;
            });
        outLayoutDesc.mLayoutHash =
            BuildLayoutHash(outLayoutDesc.mEntries, outLayoutDesc.mSetIndex);
        return true;
    }

    auto ResolveConstantBufferBindingByName(const RenderCore::FShaderRegistry& registry,
        const TVector<RenderCore::FShaderRegistry::FShaderKey>& shaderKeys, FStringView cbufferName,
        u32& outSetIndex, u32& outBinding, Rhi::ERhiShaderStageFlags& outVisibility) -> bool {
        bool found    = false;
        outSetIndex   = 0U;
        outBinding    = 0U;
        outVisibility = Rhi::ERhiShaderStageFlags::None;

        for (const auto& shaderKey : shaderKeys) {
            if (!shaderKey.IsValid()) {
                continue;
            }

            const auto shader = registry.FindShader(shaderKey);
            if (!shader) {
                continue;
            }

            const auto stageFlags = ToRhiShaderStageFlags(shader->GetDesc().mStage);
            for (const auto& cbuffer : shader->GetDesc().mReflection.mConstantBuffers) {
                if (!Core::Utility::String::EqualsIgnoreCase(cbuffer.mName.ToView(), cbufferName)) {
                    continue;
                }

                if (!found) {
                    found         = true;
                    outSetIndex   = cbuffer.mSet;
                    outBinding    = cbuffer.mBinding;
                    outVisibility = stageFlags;
                    continue;
                }

                if (outSetIndex != cbuffer.mSet || outBinding != cbuffer.mBinding) {
                    return false;
                }
                outVisibility = OrShaderStageFlags(outVisibility, stageFlags);
            }
        }

        return found;
    }

    auto ResolveResourceBindingByName(const RenderCore::FShaderRegistry& registry,
        const TVector<RenderCore::FShaderRegistry::FShaderKey>&          shaderKeys,
        FStringView resourceName, Rhi::ERhiBindingType expectedType, u32& outSetIndex,
        u32& outBinding, Rhi::ERhiShaderStageFlags& outVisibility) -> bool {
        bool found    = false;
        outSetIndex   = 0U;
        outBinding    = 0U;
        outVisibility = Rhi::ERhiShaderStageFlags::None;

        for (const auto& shaderKey : shaderKeys) {
            if (!shaderKey.IsValid()) {
                continue;
            }

            const auto shader = registry.FindShader(shaderKey);
            if (!shader) {
                continue;
            }

            const auto stageFlags = ToRhiShaderStageFlags(shader->GetDesc().mStage);
            for (const auto& resource : shader->GetDesc().mReflection.mResources) {
                if (!Core::Utility::String::EqualsIgnoreCase(
                        resource.mName.ToView(), resourceName)) {
                    continue;
                }
                if (ToRhiBindingType(resource) != expectedType) {
                    continue;
                }

                if (!found) {
                    found         = true;
                    outSetIndex   = resource.mSet;
                    outBinding    = resource.mBinding;
                    outVisibility = stageFlags;
                    continue;
                }

                if (outSetIndex != resource.mSet || outBinding != resource.mBinding) {
                    return false;
                }
                outVisibility = OrShaderStageFlags(outVisibility, stageFlags);
            }
        }

        return found;
    }

    auto HashBindingName(FStringView name) noexcept -> u32 {
        if (name.IsEmpty()) {
            return 0U;
        }

        constexpr u32 kFnvOffset32 = 2166136261U;
        constexpr u32 kFnvPrime32  = 16777619U;

        u32           hash   = kFnvOffset32;
        const auto*   data   = name.Data();
        const usize   length = name.Length();
        for (usize i = 0U; i < length; ++i) {
            const auto lowered = Core::Algorithm::ToLowerChar(data[i]);
            hash ^= static_cast<u32>(static_cast<FStringView::TUnsigned>(lowered));
            hash *= kFnvPrime32;
        }
        return hash;
    }

    auto BuildBindingLookupTable(const RenderCore::FShaderRegistry& registry,
        const TVector<RenderCore::FShaderRegistry::FShaderKey>& shaderKeys, u32 setIndex,
        const Rhi::FRhiBindGroupLayout* layout, FBindingLookupTable& outTable) -> bool {
        outTable.Reset();
        outTable.mSetIndex = setIndex;
        outTable.mLayout   = layout;

        for (const auto& shaderKey : shaderKeys) {
            if (!shaderKey.IsValid()) {
                continue;
            }

            const auto shader = registry.FindShader(shaderKey);
            if (!shader) {
                continue;
            }

            const auto& reflection = shader->GetDesc().mReflection;
            for (const auto& cbuffer : reflection.mConstantBuffers) {
                if (cbuffer.mSet != setIndex) {
                    continue;
                }
                const auto key = MakeLookupKey(
                    HashBindingName(cbuffer.mName.ToView()), Rhi::ERhiBindingType::ConstantBuffer);
                const auto it = outTable.mBindingByKey.FindIt(key);
                if (it != outTable.mBindingByKey.end()) {
                    if (it->second != cbuffer.mBinding) {
                        return false;
                    }
                    continue;
                }
                if (!LayoutHasBinding(
                        layout, cbuffer.mBinding, Rhi::ERhiBindingType::ConstantBuffer)) {
                    return false;
                }
                outTable.mBindingByKey[key] = cbuffer.mBinding;
            }

            for (const auto& resource : reflection.mResources) {
                if (resource.mSet != setIndex) {
                    continue;
                }
                const auto type = ToRhiBindingType(resource);
                const auto key  = MakeLookupKey(HashBindingName(resource.mName.ToView()), type);
                const auto it   = outTable.mBindingByKey.FindIt(key);
                if (it != outTable.mBindingByKey.end()) {
                    if (it->second != resource.mBinding) {
                        return false;
                    }
                    continue;
                }
                if (!LayoutHasBinding(layout, resource.mBinding, type)) {
                    return false;
                }
                outTable.mBindingByKey[key] = resource.mBinding;
            }
        }

        return !outTable.mBindingByKey.IsEmpty();
    }

    auto BuildBindingLookupTableFromShaders(const TVector<Rhi::FRhiShader*>& shaders, u32 setIndex,
        const Rhi::FRhiBindGroupLayout* layout, FBindingLookupTable& outTable) -> bool {
        outTable.Reset();
        outTable.mSetIndex = setIndex;
        outTable.mLayout   = layout;

        for (const auto* shader : shaders) {
            if (shader == nullptr) {
                continue;
            }

            const auto& reflection = shader->GetDesc().mReflection;
            for (const auto& cbuffer : reflection.mConstantBuffers) {
                if (cbuffer.mSet != setIndex) {
                    continue;
                }
                const auto key = MakeLookupKey(
                    HashBindingName(cbuffer.mName.ToView()), Rhi::ERhiBindingType::ConstantBuffer);
                const auto it = outTable.mBindingByKey.FindIt(key);
                if (it != outTable.mBindingByKey.end()) {
                    if (it->second != cbuffer.mBinding) {
                        return false;
                    }
                    continue;
                }
                if (!LayoutHasBinding(
                        layout, cbuffer.mBinding, Rhi::ERhiBindingType::ConstantBuffer)) {
                    return false;
                }
                outTable.mBindingByKey[key] = cbuffer.mBinding;
            }

            for (const auto& resource : reflection.mResources) {
                if (resource.mSet != setIndex) {
                    continue;
                }
                const auto type = ToRhiBindingType(resource);
                const auto key  = MakeLookupKey(HashBindingName(resource.mName.ToView()), type);
                const auto it   = outTable.mBindingByKey.FindIt(key);
                if (it != outTable.mBindingByKey.end()) {
                    if (it->second != resource.mBinding) {
                        return false;
                    }
                    continue;
                }
                if (!LayoutHasBinding(layout, resource.mBinding, type)) {
                    return false;
                }
                outTable.mBindingByKey[key] = resource.mBinding;
            }
        }

        return !outTable.mBindingByKey.IsEmpty();
    }

    auto FindBindingByNameHash(const FBindingLookupTable& table, u32 nameHash,
        Rhi::ERhiBindingType type, u32& outBinding) -> bool {
        outBinding    = kInvalidBinding;
        const auto it = table.mBindingByKey.FindIt(MakeLookupKey(nameHash, type));
        if (it == table.mBindingByKey.end()) {
            return false;
        }
        outBinding = it->second;
        return true;
    }

    void FBindGroupBuilder::Reset(const Rhi::FRhiBindGroupLayout* layout) noexcept {
        mLayout = layout;
        mEntries.Clear();
    }

    auto FBindGroupBuilder::HasLayoutBinding(u32 binding, Rhi::ERhiBindingType type) const -> bool {
        if (mLayout == nullptr) {
            return false;
        }
        for (const auto& entry : mLayout->GetDesc().mEntries) {
            if (entry.mBinding == binding && entry.mType == type) {
                return true;
            }
        }
        return false;
    }

    auto FBindGroupBuilder::HasEntry(u32 binding, Rhi::ERhiBindingType type) const -> bool {
        for (const auto& entry : mEntries) {
            if (entry.mBinding == binding && entry.mType == type) {
                return true;
            }
        }
        return false;
    }

    auto FBindGroupBuilder::AddBuffer(u32 binding, Rhi::FRhiBuffer* buffer, u64 offset, u64 size)
        -> bool {
        if (!HasLayoutBinding(binding, Rhi::ERhiBindingType::ConstantBuffer)
            || HasEntry(binding, Rhi::ERhiBindingType::ConstantBuffer)) {
            return false;
        }
        Rhi::FRhiBindGroupEntry entry{};
        entry.mBinding = binding;
        entry.mType    = Rhi::ERhiBindingType::ConstantBuffer;
        entry.mBuffer  = buffer;
        entry.mOffset  = offset;
        entry.mSize    = size;
        mEntries.PushBack(entry);
        return true;
    }

    auto FBindGroupBuilder::AddTexture(u32 binding, Rhi::FRhiTexture* texture) -> bool {
        if (!HasLayoutBinding(binding, Rhi::ERhiBindingType::SampledTexture)
            || HasEntry(binding, Rhi::ERhiBindingType::SampledTexture)) {
            return false;
        }
        Rhi::FRhiBindGroupEntry entry{};
        entry.mBinding = binding;
        entry.mType    = Rhi::ERhiBindingType::SampledTexture;
        entry.mTexture = texture;
        mEntries.PushBack(entry);
        return true;
    }

    auto FBindGroupBuilder::AddStorageTexture(u32 binding, Rhi::FRhiTexture* texture) -> bool {
        if (!HasLayoutBinding(binding, Rhi::ERhiBindingType::StorageTexture)
            || HasEntry(binding, Rhi::ERhiBindingType::StorageTexture)) {
            return false;
        }
        Rhi::FRhiBindGroupEntry entry{};
        entry.mBinding = binding;
        entry.mType    = Rhi::ERhiBindingType::StorageTexture;
        entry.mTexture = texture;
        mEntries.PushBack(entry);
        return true;
    }

    auto FBindGroupBuilder::AddSampler(u32 binding, Rhi::FRhiSampler* sampler) -> bool {
        if (!HasLayoutBinding(binding, Rhi::ERhiBindingType::Sampler)
            || HasEntry(binding, Rhi::ERhiBindingType::Sampler)) {
            return false;
        }
        Rhi::FRhiBindGroupEntry entry{};
        entry.mBinding = binding;
        entry.mType    = Rhi::ERhiBindingType::Sampler;
        entry.mSampler = sampler;
        mEntries.PushBack(entry);
        return true;
    }

    auto FBindGroupBuilder::Build(Rhi::FRhiBindGroupDesc& outDesc) const -> bool {
        if (mLayout == nullptr) {
            return false;
        }
        for (const auto& layoutEntry : mLayout->GetDesc().mEntries) {
            bool found = false;
            for (const auto& entry : mEntries) {
                if (entry.mBinding == layoutEntry.mBinding && entry.mType == layoutEntry.mType) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
            }
        }

        outDesc          = {};
        outDesc.mLayout  = const_cast<Rhi::FRhiBindGroupLayout*>(mLayout);
        outDesc.mEntries = mEntries;
        return true;
    }
} // namespace AltinaEngine::RenderCore::ShaderBinding
