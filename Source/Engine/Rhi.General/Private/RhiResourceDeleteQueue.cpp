#include "Rhi/RhiResourceDeleteQueue.h"

#include "Rhi/RhiResource.h"

namespace AltinaEngine::Rhi {

    void FRhiResourceDeleteQueue::Enqueue(FRhiResource* resource, u64 serial) {
        if (!resource) {
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

        usize writeIndex = 0;
        for (usize index = 0; index < mEntries.Size(); ++index) {
            const auto& entry = mEntries[index];
            if (entry.mResource && entry.mSerial <= completedSerial) {
                entry.mResource->DestroySelf();
                continue;
            }

            if (writeIndex != index) {
                mEntries[writeIndex] = entry;
            }
            ++writeIndex;
        }

        mEntries.Resize(writeIndex);
    }

    void FRhiResourceDeleteQueue::Flush() {
        for (auto& entry : mEntries) {
            if (entry.mResource) {
                entry.mResource->DestroySelf();
            }
        }
        mEntries.Clear();
    }

    auto FRhiResourceDeleteQueue::GetPendingCount() const noexcept -> u32 {
        return static_cast<u32>(mEntries.Size());
    }

} // namespace AltinaEngine::Rhi
