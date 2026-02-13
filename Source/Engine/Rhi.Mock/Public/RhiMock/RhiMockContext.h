#pragma once

#include "RhiMockAPI.h"
#include "Rhi/RhiContext.h"
#include "Container/SmartPtr.h"
#include "Container/Vector.h"

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::TShared;
    using Container::TVector;

    struct FRhiMockAdapterConfig {
        FRhiAdapterDesc       mDesc;
        FRhiSupportedFeatures mFeatures;
        FRhiSupportedLimits   mLimits;
    };

    struct FRhiMockCounters {
        u32                mInitializeCalls   = 0U;
        u32                mShutdownCalls     = 0U;
        u32                mEnumerateCalls    = 0U;
        u32                mCreateDeviceCalls = 0U;
        u32                mDeviceCreated     = 0U;
        u32                mDeviceDestroyed   = 0U;
        u32                mResourceCreated   = 0U;
        u32                mResourceDestroyed = 0U;

        [[nodiscard]] auto GetDeviceLiveCount() const noexcept -> u32 {
            return (mDeviceCreated >= mDeviceDestroyed) ? (mDeviceCreated - mDeviceDestroyed) : 0U;
        }

        [[nodiscard]] auto GetResourceLiveCount() const noexcept -> u32 {
            return (mResourceCreated >= mResourceDestroyed)
                ? (mResourceCreated - mResourceDestroyed)
                : 0U;
        }
    };

    class AE_RHI_MOCK_API FRhiMockContext final : public FRhiContext {
    public:
        FRhiMockContext();
        ~FRhiMockContext() override;

        void               AddAdapter(const FRhiAdapterDesc& desc,
                          const FRhiSupportedFeatures&       features = FRhiSupportedFeatures{},
                          const FRhiSupportedLimits&         limits   = FRhiSupportedLimits{});
        void               AddAdapter(const FRhiMockAdapterConfig& config);
        void               SetAdapters(TVector<FRhiMockAdapterConfig> configs);
        void               ClearAdapters();

        void               MarkAdaptersDirty();

        [[nodiscard]] auto GetCounters() const noexcept -> const FRhiMockCounters&;
        [[nodiscard]] auto GetInitializeCallCount() const noexcept -> u32;
        [[nodiscard]] auto GetShutdownCallCount() const noexcept -> u32;
        [[nodiscard]] auto GetEnumerateAdapterCallCount() const noexcept -> u32;
        [[nodiscard]] auto GetCreateDeviceCallCount() const noexcept -> u32;
        [[nodiscard]] auto GetDeviceCreatedCount() const noexcept -> u32;
        [[nodiscard]] auto GetDeviceDestroyedCount() const noexcept -> u32;
        [[nodiscard]] auto GetDeviceLiveCount() const noexcept -> u32;
        [[nodiscard]] auto GetResourceCreatedCount() const noexcept -> u32;
        [[nodiscard]] auto GetResourceDestroyedCount() const noexcept -> u32;
        [[nodiscard]] auto GetResourceLiveCount() const noexcept -> u32;

    protected:
        auto InitializeBackend(const FRhiInitDesc& desc) -> bool override;
        void ShutdownBackend() override;
        void EnumerateAdaptersInternal(TVector<TShared<FRhiAdapter>>& outAdapters) override;
        auto CreateDeviceInternal(const TShared<FRhiAdapter>& adapter, const FRhiDeviceDesc& desc)
            -> TShared<FRhiDevice> override;

    private:
        TVector<FRhiMockAdapterConfig> mAdapterConfigs;
        TShared<FRhiMockCounters>      mCounters;
    };

} // namespace AltinaEngine::Rhi
