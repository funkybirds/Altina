#pragma once

#include "RhiGeneralAPI.h"
#include "Container/Vector.h"
#include "Types/NonCopyable.h"

using AltinaEngine::Core::Container::TVector;
namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    class FRhiResource;

    class AE_RHI_GENERAL_API FRhiResourceDeleteQueue : public FNonCopyableClass {
    public:
        void               Enqueue(FRhiResource* resource, u64 serial);
        void               Process(u64 completedSerial);
        void               Flush();

        [[nodiscard]] auto GetPendingCount() const noexcept -> u32;

    private:
        struct FEntry {
            FRhiResource* mResource = nullptr;
            u64           mSerial   = 0ULL;
        };

        TVector<FEntry> mEntries;
    };

} // namespace AltinaEngine::Rhi
