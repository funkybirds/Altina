#pragma once

#include "RhiD3D11API.h"
#include "Rhi/RhiContext.h"
#include "Container/SmartPtr.h"
#include "Container/Vector.h"

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::TShared;
    using Container::TVector;

    struct FRhiD3D11ContextState;

    class AE_RHI_D3D11_API FRhiD3D11Context final : public FRhiContext {
    public:
        FRhiD3D11Context();
        ~FRhiD3D11Context() override;

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
        Container::TOwner<FRhiD3D11ContextState> mState;
    };

} // namespace AltinaEngine::Rhi
