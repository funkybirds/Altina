#include "Shader/ShaderBindingUtility.h"

#include "Algorithm/Sort.h"
#include "Utility/String/StringViewUtility.h"

namespace AltinaEngine::RenderCore::ShaderBinding {
    namespace {
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
} // namespace AltinaEngine::RenderCore::ShaderBinding
