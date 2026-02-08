#pragma once

#include "RhiD3D11API.h"
#include "Rhi/RhiContext.h"
#include "Container/SmartPtr.h"
#include "Container/Vector.h"

namespace AltinaEngine::Rhi {
    using Core::Container::TShared;
    using Core::Container::TVector;

    struct FRhiD3D11ContextState;

    class AE_RHI_D3D11_API FRhiD3D11Context final : public FRhiContext {
    public:
        FRhiD3D11Context();
        ~FRhiD3D11Context() override;

    protected:
        auto InitializeBackend(const FRhiInitDesc& desc) -> bool override;
        void ShutdownBackend() override;
        void EnumerateAdaptersInternal(TVector<TShared<FRhiAdapter>>& outAdapters) override;
        auto CreateDeviceInternal(
            const TShared<FRhiAdapter>& adapter, const FRhiDeviceDesc& desc)
            -> TShared<FRhiDevice> override;

    private:
        Core::Container::TOwner<FRhiD3D11ContextState> mState;
    };

} // namespace AltinaEngine::Rhi
