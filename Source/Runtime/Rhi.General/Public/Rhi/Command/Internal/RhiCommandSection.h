#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiCommandList.h"
#include "Rhi/RhiCommandPool.h"
#include "Rhi/RhiRefs.h"
#include "Rhi/RhiStructs.h"
#include "Container/Queue.h"
#include "Container/ThreadSafeQueue.h"
#include "Container/Vector.h"
#include "Container/SmartPtr.h"
#include "Threading/Atomic.h"
#include "Threading/Mutex.h"
#include "Types/NonCopyable.h"
#include "Types/Traits.h"

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    namespace Threading = Core::Threading;
    using AltinaEngine::Move;
    using Container::TOwner;
    using Container::TQueue;
    using Container::TThreadSafeQueue;
    using Container::TVector;

    struct FRhiCommandSubsection final {
        FRhiCommandListRef       mCommandList;
        FRhiCommandHostSyncPoint mHostDependency;
        Threading::TAtomic<u32>  mPendingDependencyCount{ 0U };
        ERhiCommandSectionState  mState   = ERhiCommandSectionState::Pending;
        bool                     mTouched = false;

        void                     Reset() {
            mHostDependency = {};
            mPendingDependencyCount.Store(0U);
            mState   = ERhiCommandSectionState::Pending;
            mTouched = false;
        }
    };

    struct FRhiCommandSection final {
        FRhiCommandSubsection    mUploading;
        FRhiCommandSubsection    mExecution;
        TVector<FRhiQueueWait>   mExternalWaits;
        TVector<FRhiQueueSignal> mSignals;
        FRhiFence*               mFence          = nullptr;
        u64                      mFenceValue     = 0ULL;
        FRhiSemaphore*           mSemaphore      = nullptr;
        u64                      mSemaphoreValue = 0ULL;
        u64                      mSubmittedFrame = 0ULL;
        u64                      mSerial         = 0ULL;
        ERhiCommandSectionState  mState          = ERhiCommandSectionState::Pending;

        void                     Reset() {
            mUploading.Reset();
            mExecution.Reset();
            mExternalWaits.Clear();
            mSignals.Clear();
            mFence          = nullptr;
            mFenceValue     = 0ULL;
            mSemaphore      = nullptr;
            mSemaphoreValue = 0ULL;
            mSubmittedFrame = 0ULL;
            mSerial         = 0ULL;
            mState          = ERhiCommandSectionState::Pending;
        }

        [[nodiscard]] auto IsEmpty() const noexcept -> bool {
            return !mUploading.mTouched && !mExecution.mTouched;
        }
    };

    class FRhiCommandSectionPool : public FNonCopyableClass {
    public:
        FRhiCommandSectionPool() = default;
        ~FRhiCommandSectionPool() override {
            for (auto& section : mSections) {
                section.Reset();
            }
            mSections.Clear();
        }

        template <typename TAcquire>
        auto Acquire(TAcquire&& acquireSubsection) -> FRhiCommandSection* {
            FRhiCommandSection* section = nullptr;
            if (!mRecycled.TryPop(section) || section == nullptr) {
                TOwner<FRhiCommandSection> newSection = Container::MakeUnique<FRhiCommandSection>();
                newSection->mUploading.mCommandList   = acquireSubsection(true);
                newSection->mExecution.mCommandList   = acquireSubsection(false);
                section                               = newSection.Get();
                mSections.PushBack(Move(newSection));
            }
            if (section) {
                section->Reset();
            }
            return section;
        }

        void Recycle(FRhiCommandSection* section) {
            if (section == nullptr) {
                return;
            }
            section->mState = ERhiCommandSectionState::Recyclable;
            mRecycled.Push(section);
        }

    private:
        TVector<TOwner<FRhiCommandSection>>   mSections;
        TThreadSafeQueue<FRhiCommandSection*> mRecycled;
    };

    class FRhiCommandSubmissionProcessor : public FNonCopyableClass {
    public:
        FRhiCommandSubmissionProcessor()           = default;
        ~FRhiCommandSubmissionProcessor() override = default;

        void EnqueueWaiting(FRhiCommandSection* section) {
            if (section == nullptr) {
                return;
            }
            section->mState = ERhiCommandSectionState::Enqueued;
            mWaitingQueue.Push(section);
        }

        template <typename TSubmit> auto Process(TSubmit&& submit) -> FRhiCommandSubmissionStamp {
            FRhiCommandSubmissionStamp  lastStamp{};
            FRhiCommandSection*         section = nullptr;
            TQueue<FRhiCommandSection*> deferred;
            while (!mWaitingQueue.IsEmpty()) {
                section = mWaitingQueue.Front();
                mWaitingQueue.Pop();
                if (section == nullptr) {
                    continue;
                }
                if (section->mExecution.mPendingDependencyCount.Load() != 0U) {
                    deferred.Push(section);
                    continue;
                }

                section->mState            = ERhiCommandSectionState::HostSubmitted;
                section->mExecution.mState = ERhiCommandSectionState::HostSubmitted;
                lastStamp                  = submit(*section);
                section->mState            = ERhiCommandSectionState::DeviceExecuted;
                section->mExecution.mState = ERhiCommandSectionState::DeviceExecuted;
            }

            while (!deferred.IsEmpty()) {
                mWaitingQueue.Push(deferred.Front());
                deferred.Pop();
            }
            return lastStamp;
        }

    private:
        TQueue<FRhiCommandSection*> mWaitingQueue;
    };
} // namespace AltinaEngine::Rhi
