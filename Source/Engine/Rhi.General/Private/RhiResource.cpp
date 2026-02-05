#include "Rhi/RhiResource.h"

#include "Rhi/RhiResourceDeleteQueue.h"
#include "Threading/Atomic.h"

namespace AltinaEngine::Rhi {
    using Core::Threading::EMemoryOrder;

    FRhiResource::FRhiResource(FRhiResourceDeleteQueue* deleteQueue) noexcept
        : mRefCount(1U), mDeleteQueue(deleteQueue) {}

    void FRhiResource::AddRef() noexcept {
        mRefCount.FetchAdd(1U, EMemoryOrder::AcquireRelease);
    }

    void FRhiResource::Release() noexcept {
        if (mRefCount.FetchSub(1U, EMemoryOrder::AcquireRelease) == 1U) {
            if (mDeleteQueue) {
                mDeleteQueue->Enqueue(this, mRetireSerial);
            } else {
                DestroySelf();
            }
        }
    }

    auto FRhiResource::GetRefCount() const noexcept -> u32 {
        return mRefCount.Load(EMemoryOrder::Acquire);
    }

    void FRhiResource::SetDeleteQueue(FRhiResourceDeleteQueue* queue) noexcept {
        mDeleteQueue = queue;
    }

    auto FRhiResource::GetDeleteQueue() const noexcept -> FRhiResourceDeleteQueue* {
        return mDeleteQueue;
    }

    void FRhiResource::SetRetireSerial(u64 serial) noexcept {
        mRetireSerial = serial;
    }

    auto FRhiResource::GetRetireSerial() const noexcept -> u64 {
        return mRetireSerial;
    }

    void FRhiResource::DestroySelf() noexcept {
        delete this;
    }

} // namespace AltinaEngine::Rhi
