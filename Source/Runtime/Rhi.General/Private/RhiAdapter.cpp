#include "Rhi/RhiAdapter.h"

#include "Types/Traits.h"

namespace AltinaEngine::Rhi {
    namespace {
        constexpr u64 kAdapterTypeScoreShift = 56ULL;

        auto GetTypeScore(const FRhiAdapterDesc& desc, ERhiGpuPreference preference) -> u64 {
            switch (preference) {
                case ERhiGpuPreference::HighPerformance:
                    if (desc.IsDiscrete()) {
                        return 3ULL;
                    }
                    if (desc.IsIntegrated()) {
                        return 2ULL;
                    }
                    return desc.IsValid() ? 1ULL : 0ULL;
                case ERhiGpuPreference::LowPower:
                    if (desc.IsIntegrated()) {
                        return 3ULL;
                    }
                    if (desc.IsDiscrete()) {
                        return 2ULL;
                    }
                    return desc.IsValid() ? 1ULL : 0ULL;
                case ERhiGpuPreference::Auto:
                default:
                    if (desc.IsDiscrete()) {
                        return 3ULL;
                    }
                    if (desc.IsIntegrated()) {
                        return 2ULL;
                    }
                    return desc.IsValid() ? 1ULL : 0ULL;
            }
        }
    } // namespace

    FRhiAdapter::FRhiAdapter(FRhiAdapterDesc desc) : mDesc(Move(desc)) {}

    auto FRhiAdapter::GetDesc() const noexcept -> const FRhiAdapterDesc& { return mDesc; }

    auto FRhiAdapter::GetName() const noexcept -> FStringView { return mDesc.GetName(); }

    auto FRhiAdapter::IsValid() const noexcept -> bool { return mDesc.IsValid(); }

    auto FRhiAdapter::IsDiscrete() const noexcept -> bool { return mDesc.IsDiscrete(); }

    auto FRhiAdapter::IsIntegrated() const noexcept -> bool { return mDesc.IsIntegrated(); }

    auto FRhiAdapter::IsSoftware() const noexcept -> bool { return mDesc.IsSoftware(); }

    auto FRhiAdapter::GetPreferenceScore(ERhiGpuPreference preference) const noexcept -> u64 {
        if (!mDesc.IsValid()) {
            return 0ULL;
        }

        u64 typeScore = GetTypeScore(mDesc, preference);
        if (mDesc.IsSoftware()) {
            typeScore = 0ULL;
        }

        const u64 memoryScore = mDesc.GetTotalMemoryBytes();
        return (typeScore << kAdapterTypeScoreShift) + memoryScore;
    }

} // namespace AltinaEngine::Rhi
