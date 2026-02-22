#pragma once

#include "RhiVulkanAPI.h"
#include "Rhi/RhiContext.h"
#include "Container/SmartPtr.h"
#include "Container/Vector.h"

using AltinaEngine::Core::Container::TOwner;
namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::TShared;
    using Container::TVector;

    struct FRhiVulkanContextState;

    class AE_RHI_VULKAN_API FRhiVulkanContext final : public FRhiContext {
    public:
        FRhiVulkanContext();
        ~FRhiVulkanContext() override;

        [[nodiscard]] auto AdjustProjectionMatrix(
            const Core::Math::FMatrix4x4f& matrix) const noexcept
            -> Core::Math::FMatrix4x4f override;

    protected:
        auto InitializeBackend(const FRhiInitDesc& desc) -> bool override;
        void ShutdownBackend() override;
        void EnumerateAdaptersInternal(TVector<TShared<FRhiAdapter>>& outAdapters) override;
        auto CreateDeviceInternal(const TShared<FRhiAdapter>& adapter, const FRhiDeviceDesc& desc)
            -> TShared<FRhiDevice> override;

    private:
        TOwner<FRhiVulkanContextState> mState;
    };

} // namespace AltinaEngine::Rhi
