#include "Rhi/RhiResourceDeleteQueue.h"

#include "Rhi/RhiResource.h"

namespace AltinaEngine::Rhi {

    void FRhiResourceDeleteQueue::Enqueue(FRhiResource* resource, u64 serial) {
        if (resource == nullptr) {
            return;
        }

        FEntry entry;
        entry.mResource = resource;
        entry.mSerial   = serial;
        mEntries.PushBack(entry);
    }

    void FRhiResourceDeleteQueue::Process(u64 completedSerial) {
        if (mEntries.IsEmpty()) {
            return;
        }

        TVector<FEntry> survivors;
        survivors.Reserve(mEntries.Size());

        const usize initialCount = mEntries.Size();
        for (usize index = 0; index < initialCount; ++index) {
            const FEntry entry = mEntries[index];
            if ((entry.mResource != nullptr) && entry.mSerial <= completedSerial) {
                entry.mResource->DestroySelf();
                continue;
            }
            survivors.PushBack(entry);
        }

        for (usize index = initialCount; index < mEntries.Size(); ++index) {
            survivors.PushBack(mEntries[index]);
        }

        mEntries = Move(survivors);
    }

    void FRhiResourceDeleteQueue::Flush() {
        while (!mEntries.IsEmpty()) {
            TVector<FEntry> batch = Move(mEntries);
            mEntries.Clear();

            for (const auto& entry : batch) {
                if (entry.mResource != nullptr) {
                    entry.mResource->DestroySelf();
                }
            }
        }
    }

    auto FRhiResourceDeleteQueue::GetPendingCount() const noexcept -> u32 {
        return static_cast<u32>(mEntries.Size());
    }

} // namespace AltinaEngine::Rhi
