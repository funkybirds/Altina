#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/Command/RhiCmdContextOps.h"
#include "Container/StringView.h"

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::FStringView;

    class FRhiDebugMarker {
    public:
        FRhiDebugMarker() = default;

        explicit FRhiDebugMarker(IRhiCmdContextOps* cmdCtx, FStringView markerText)
            : mCmdCtx(cmdCtx), mActive(false) {
            if (mCmdCtx != nullptr && !markerText.IsEmpty()) {
                mCmdCtx->RHIPushDebugMarker(markerText);
                mActive = true;
            }
        }

        FRhiDebugMarker(IRhiCmdContextOps& cmdCtx, FStringView markerText)
            : FRhiDebugMarker(&cmdCtx, markerText) {}

        ~FRhiDebugMarker() {
            if (mCmdCtx != nullptr && mActive) {
                mCmdCtx->RHIPopDebugMarker();
            }
        }

        FRhiDebugMarker(const FRhiDebugMarker&)                    = delete;
        auto operator=(const FRhiDebugMarker&) -> FRhiDebugMarker& = delete;
        FRhiDebugMarker(FRhiDebugMarker&&)                         = delete;
        auto operator=(FRhiDebugMarker&&) -> FRhiDebugMarker&      = delete;

    private:
        IRhiCmdContextOps* mCmdCtx = nullptr;
        bool               mActive = false;
    };
} // namespace AltinaEngine::Rhi
