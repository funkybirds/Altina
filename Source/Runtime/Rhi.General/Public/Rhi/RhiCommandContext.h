#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiResource.h"
#include "Rhi/Command/RhiCmdContextOps.h"
#include "Rhi/RhiStructs.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Container/Vector.h"

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::FStringView;
    using Container::TVector;

    class AE_RHI_GENERAL_API FRhiCommandContext : public FRhiResource, public IRhiCmdContextOps {
    public:
        explicit FRhiCommandContext(const FRhiCommandContextDesc& desc,
            FRhiResourceDeleteQueue*                              deleteQueue = nullptr) noexcept;

        ~FRhiCommandContext() override;

        FRhiCommandContext(const FRhiCommandContext&)                                  = delete;
        FRhiCommandContext(FRhiCommandContext&&)                                       = delete;
        auto               operator=(const FRhiCommandContext&) -> FRhiCommandContext& = delete;
        auto               operator=(FRhiCommandContext&&) -> FRhiCommandContext&      = delete;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiCommandContextDesc& {
            return mDesc;
        }
        [[nodiscard]] auto GetQueueType() const noexcept -> ERhiQueueType {
            return mDesc.mQueueType;
        }
        [[nodiscard]] auto GetListType() const noexcept -> ERhiCommandListType {
            return mDesc.mListType;
        }
        [[nodiscard]] auto GetDebugName() const noexcept -> FStringView {
            return mDesc.mDebugName.ToView();
        }
        void SetDebugName(FStringView name) {
            mDesc.mDebugName.Clear();
            if (!name.IsEmpty()) {
                mDesc.mDebugName.Append(name.Data(), name.Length());
            }
        }

        virtual auto RHISubmitActiveSection(const FRhiCommandContextSubmitInfo& submitInfo)
            -> FRhiCommandSubmissionStamp;
        virtual auto RHIFlushContextHost(const FRhiCommandContextSubmitInfo& submitInfo)
            -> FRhiCommandHostSyncPoint;
        virtual auto RHIFlushContextDevice(const FRhiCommandContextSubmitInfo& submitInfo)
            -> FRhiCommandSubmissionStamp;
        virtual auto RHISwitchContextCapability(ERhiContextCapability capability)
            -> FRhiCommandSubmissionStamp;

        void RHIPushDebugMarker(FStringView text) override;
        void RHIPopDebugMarker() override;
        void RHIInsertDebugMarker(FStringView text) override;

    protected:
        virtual void       RHIPushDebugMarkerNative(FStringView text);
        virtual void       RHIPopDebugMarkerNative();
        virtual void       RHIInsertDebugMarkerNative(FStringView text);

        void               RHIBeginSectionDebugMarkers();
        void               RHIEndSectionDebugMarkers();
        [[nodiscard]] auto HasOpenSectionDebugMarkers() const noexcept -> bool {
            return mSectionDebugMarkersOpen;
        }

    private:
        FRhiCommandContextDesc mDesc;
        TVector<FString>       mDebugMarkerStack;
        bool                   mSectionDebugMarkersOpen = false;
    };

} // namespace AltinaEngine::Rhi
