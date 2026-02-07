#include "ShaderCompiler/ShaderRhiBindings.h"

#include "Container/Vector.h"
#include <algorithm>

namespace AltinaEngine::ShaderCompiler {
    namespace {
        auto ToStageFlags(EShaderStage stage) -> Rhi::ERhiShaderStageFlags {
            using Rhi::ERhiShaderStageFlags;
            switch (stage) {
            case EShaderStage::Vertex:
                return ERhiShaderStageFlags::Vertex;
            case EShaderStage::Pixel:
                return ERhiShaderStageFlags::Pixel;
            case EShaderStage::Compute:
                return ERhiShaderStageFlags::Compute;
            case EShaderStage::Geometry:
                return ERhiShaderStageFlags::Geometry;
            case EShaderStage::Hull:
                return ERhiShaderStageFlags::Hull;
            case EShaderStage::Domain:
                return ERhiShaderStageFlags::Domain;
            case EShaderStage::Mesh:
                return ERhiShaderStageFlags::Mesh;
            case EShaderStage::Amplification:
                return ERhiShaderStageFlags::Amplification;
            case EShaderStage::Library:
            default:
                return ERhiShaderStageFlags::All;
            }
        }

        auto ToBindingType(const FShaderResourceBinding& resource) -> Rhi::ERhiBindingType {
            using Rhi::ERhiBindingType;
            switch (resource.mType) {
            case EShaderResourceType::ConstantBuffer:
                return ERhiBindingType::ConstantBuffer;
            case EShaderResourceType::Texture:
                return ERhiBindingType::SampledTexture;
            case EShaderResourceType::Sampler:
                return ERhiBindingType::Sampler;
            case EShaderResourceType::StorageBuffer:
                return (resource.mAccess == EShaderResourceAccess::ReadWrite)
                    ? ERhiBindingType::StorageBuffer
                    : ERhiBindingType::SampledBuffer;
            case EShaderResourceType::StorageTexture:
                return (resource.mAccess == EShaderResourceAccess::ReadWrite)
                    ? ERhiBindingType::StorageTexture
                    : ERhiBindingType::SampledTexture;
            case EShaderResourceType::AccelerationStructure:
                return ERhiBindingType::AccelerationStructure;
            default:
                return ERhiBindingType::SampledTexture;
            }
        }

        auto HashCombine(u64& seed, u64 value) -> void {
            constexpr u64 kPrime = 1099511628211ULL;
            seed = (seed ^ value) * kPrime;
        }

        auto BuildLayoutHash(const TVector<Rhi::FRhiBindGroupLayoutEntry>& entries, u32 setIndex)
            -> u64 {
            constexpr u64 kOffset = 1469598103934665603ULL;
            u64 hash = kOffset;
            HashCombine(hash, setIndex);
            for (const auto& entry : entries) {
                HashCombine(hash, entry.mBinding);
                HashCombine(hash, static_cast<u64>(entry.mType));
                HashCombine(hash, static_cast<u64>(entry.mVisibility));
                HashCombine(hash, entry.mArrayCount);
                HashCombine(hash, entry.mHasDynamicOffset ? 1ULL : 0ULL);
            }
            return hash;
        }

        auto BuildPipelineHash(const TVector<Rhi::FRhiBindGroupLayoutDesc>& layouts,
            const TVector<Rhi::FRhiPushConstantRange>& pushConstants) -> u64 {
            constexpr u64 kOffset = 1469598103934665603ULL;
            u64 hash = kOffset;
            for (const auto& layout : layouts) {
                HashCombine(hash, layout.mSetIndex);
                HashCombine(hash, layout.mLayoutHash);
            }
            for (const auto& range : pushConstants) {
                HashCombine(hash, range.mOffset);
                HashCombine(hash, range.mSize);
                HashCombine(hash, static_cast<u64>(range.mVisibility));
            }
            return hash;
        }
    } // namespace

    auto BuildRhiBindingLayout(const FShaderReflection& reflection, EShaderStage stage)
        -> FRhiShaderBindingLayout {
        FRhiShaderBindingLayout result{};
        const auto              stageFlags = ToStageFlags(stage);

        struct FSetEntries {
            u32                                      mSetIndex = 0U;
            TVector<Rhi::FRhiBindGroupLayoutEntry> mEntries;
        };

        TVector<FSetEntries> sets;
        sets.Reserve(reflection.mResources.Size());

        for (const auto& resource : reflection.mResources) {
            const u32 setIndex = resource.mSet;
            auto*     setPtr   = static_cast<FSetEntries*>(nullptr);
            for (auto& set : sets) {
                if (set.mSetIndex == setIndex) {
                    setPtr = &set;
                    break;
                }
            }
            if (!setPtr) {
                sets.PushBack(FSetEntries{ setIndex, {} });
                setPtr = &sets.Back();
            }

            Rhi::FRhiBindGroupLayoutEntry entry{};
            entry.mBinding        = resource.mBinding;
            entry.mType           = ToBindingType(resource);
            entry.mVisibility     = stageFlags;
            entry.mArrayCount     = 1U;
            entry.mHasDynamicOffset = false;
            setPtr->mEntries.PushBack(entry);
        }

        for (auto& set : sets) {
            auto& entries = set.mEntries;
            std::sort(entries.begin(), entries.end(),
                [](const auto& lhs, const auto& rhs) { return lhs.mBinding < rhs.mBinding; });

            Rhi::FRhiBindGroupLayoutDesc layout{};
            layout.mSetIndex   = set.mSetIndex;
            layout.mEntries    = entries;
            layout.mLayoutHash = BuildLayoutHash(layout.mEntries, layout.mSetIndex);
            result.mBindGroupLayouts.PushBack(layout);
        }

        std::sort(result.mBindGroupLayouts.begin(), result.mBindGroupLayouts.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.mSetIndex < rhs.mSetIndex; });

        if (reflection.mPushConstantBytes > 0U) {
            Rhi::FRhiPushConstantRange range{};
            range.mOffset     = 0U;
            range.mSize       = reflection.mPushConstantBytes;
            range.mVisibility = stageFlags;
            result.mPipelineLayout.mPushConstants.PushBack(range);
        }

        result.mPipelineLayout.mLayoutHash =
            BuildPipelineHash(result.mBindGroupLayouts, result.mPipelineLayout.mPushConstants);

        return result;
    }

    auto BuildRhiShaderDesc(const FShaderCompileResult& result) -> Rhi::FRhiShaderDesc {
        Rhi::FRhiShaderDesc desc{};
        desc.mStage = result.mStage;
        desc.mBytecode.mData = result.mBytecode;
        desc.mReflection = result.mReflection;
        return desc;
    }

} // namespace AltinaEngine::ShaderCompiler
