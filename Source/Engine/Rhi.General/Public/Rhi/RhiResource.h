#pragma once

#include "RhiGeneralAPI.h"
#include "Threading/Atomic.h"
#include "Types/NonCopyable.h"

namespace AltinaEngine::Rhi {
    class FRhiResourceDeleteQueue;

    class AE_RHI_GENERAL_API FRhiResource : public FNonCopyableClass {
    public:
        FRhiResource(const FRhiResource&)                                  = delete;
        FRhiResource(FRhiResource&&)                                       = delete;
        auto               operator=(const FRhiResource&) -> FRhiResource& = delete;
        auto               operator=(FRhiResource&&) -> FRhiResource&      = delete;

        void               AddRef() noexcept;
        void               Release() noexcept;

        [[nodiscard]] auto GetRefCount() const noexcept -> u32;

        void               SetDeleteQueue(FRhiResourceDeleteQueue* queue) noexcept;
        [[nodiscard]] auto GetDeleteQueue() const noexcept -> FRhiResourceDeleteQueue*;

        void               SetRetireSerial(u64 serial) noexcept;
        [[nodiscard]] auto GetRetireSerial() const noexcept -> u64;

        ~FRhiResource() override = default;

    protected:
        explicit FRhiResource(FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

    private:
        void DestroySelf() noexcept;

        friend class FRhiResourceDeleteQueue;

        Core::Threading::TAtomic<u32> mRefCount;
        FRhiResourceDeleteQueue*      mDeleteQueue  = nullptr;
        u64                           mRetireSerial = 0ULL;
    };

} // namespace AltinaEngine::Rhi
