#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiStructs.h"
#include "Types/NonCopyable.h"

namespace AltinaEngine::Rhi {
    using Core::Container::FStringView;

    class AE_RHI_GENERAL_API FRhiAdapter : public FNonCopyableClass {
    public:
        explicit FRhiAdapter(FRhiAdapterDesc desc);
        ~FRhiAdapter() override = default;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiAdapterDesc&;
        [[nodiscard]] auto GetName() const noexcept -> FStringView;

        [[nodiscard]] auto IsValid() const noexcept -> bool;
        [[nodiscard]] auto IsDiscrete() const noexcept -> bool;
        [[nodiscard]] auto IsIntegrated() const noexcept -> bool;
        [[nodiscard]] auto IsSoftware() const noexcept -> bool;

        [[nodiscard]] auto GetPreferenceScore(ERhiGpuPreference preference) const noexcept -> u64;

    protected:
        FRhiAdapterDesc mDesc;
    };

} // namespace AltinaEngine::Rhi
