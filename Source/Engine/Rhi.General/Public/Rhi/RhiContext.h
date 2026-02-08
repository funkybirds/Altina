#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiAdapter.h"
#include "Container/SmartPtr.h"
#include "Container/Vector.h"
#include "Types/NonCopyable.h"

namespace AltinaEngine::Rhi {
    using Core::Container::TShared;
    using Core::Container::TVector;

    class FRhiDevice;

    class AE_RHI_GENERAL_API FRhiContext : public FNonCopyableClass {
    public:
        FRhiContext()           = default;
        ~FRhiContext() override = default;

        auto               Init(const FRhiInitDesc& desc) -> bool;
        void               Shutdown();

        [[nodiscard]] auto IsInitialized() const noexcept -> bool { return mIsInitialized; }
        [[nodiscard]] auto GetInitDesc() const noexcept -> const FRhiInitDesc& { return mInitDesc; }

        auto               EnumerateAdapters() -> TVector<FRhiAdapterDesc>;
        [[nodiscard]] auto GetAdapterCount() const noexcept -> u32;
        [[nodiscard]] auto GetAdapterDesc(u32 index) const noexcept -> const FRhiAdapterDesc*;
        [[nodiscard]] auto GetPreferredAdapterIndex() const noexcept -> u32;

        auto CreateDevice(u32 adapterIndex, const FRhiDeviceDesc& deviceDesc = FRhiDeviceDesc{})
            -> TShared<FRhiDevice>;

    protected:
        virtual auto InitializeBackend(const FRhiInitDesc& desc) -> bool                   = 0;
        virtual void ShutdownBackend()                                                     = 0;
        virtual void EnumerateAdaptersInternal(TVector<TShared<FRhiAdapter>>& outAdapters) = 0;
        virtual auto CreateDeviceInternal(const TShared<FRhiAdapter>& adapter,
            const FRhiDeviceDesc& desc) -> TShared<FRhiDevice>                             = 0;

        void         InvalidateAdapterCache() noexcept { mAdaptersDirty = true; }

    private:
        void               RefreshAdapters();
        [[nodiscard]] auto SelectAdapterIndex(ERhiGpuPreference preference) const noexcept -> u32;

        FRhiInitDesc       mInitDesc;
        TVector<TShared<FRhiAdapter>> mAdapters;
        bool                          mIsInitialized = false;
        bool                          mAdaptersDirty = true;
    };

} // namespace AltinaEngine::Rhi
