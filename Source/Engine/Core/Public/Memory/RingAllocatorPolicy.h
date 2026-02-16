#pragma once

#include "Container/Deque.h"
#include "Types/Aliases.h"

using AltinaEngine::Core::Container::TDeque;
namespace AltinaEngine::Core::Memory {
    namespace Container = Core::Container;
    struct FRingAllocation {
        u64                mOffset = 0ULL;
        u64                mSize   = 0ULL;
        u64                mTag    = 0ULL;

        [[nodiscard]] auto IsValid() const noexcept -> bool { return mSize != 0ULL; }
    };

    class FRingAllocatorPolicy {
    public:
        FRingAllocatorPolicy() = default;
        explicit FRingAllocatorPolicy(u64 capacityBytes) { Init(capacityBytes); }

        void Init(u64 capacityBytes) {
            mCapacity = capacityBytes;
            Reset();
        }

        void Reset() {
            mHead = 0ULL;
            mTail = 0ULL;
            mQueue.Clear();
        }

        [[nodiscard]] auto GetCapacity() const noexcept -> u64 { return mCapacity; }
        [[nodiscard]] auto GetHead() const noexcept -> u64 { return mHead; }
        [[nodiscard]] auto GetTail() const noexcept -> u64 { return mTail; }

        auto Allocate(u64 sizeBytes, u64 alignment, u64 tag = 0ULL) -> FRingAllocation {
            if (mCapacity == 0ULL || sizeBytes == 0ULL) {
                return {};
            }

            const u64 align       = (alignment == 0ULL) ? 1ULL : alignment;
            const u64 alignedHead = AlignUp(mHead, align);

            if (mHead < mTail) {
                if (alignedHead + sizeBytes > mTail) {
                    return {};
                }
                return CommitAllocation(alignedHead, sizeBytes, tag);
            }

            if (alignedHead + sizeBytes <= mCapacity) {
                return CommitAllocation(alignedHead, sizeBytes, tag);
            }

            const u64 wrappedHead = AlignUp(0ULL, align);
            if (wrappedHead + sizeBytes > mTail) {
                return {};
            }

            if (mHead < mCapacity) {
                mQueue.PushBack(FQueueEntry{ mCapacity, tag });
            }

            return CommitAllocation(wrappedHead, sizeBytes, tag);
        }

        void ReleaseUpTo(u64 tag) {
            while (!mQueue.IsEmpty()) {
                const auto& entry = mQueue.Front();
                if (entry.mTag > tag) {
                    break;
                }
                mTail = entry.mEnd;
                mQueue.PopFront();
                if (mTail >= mCapacity) {
                    mTail = 0ULL;
                }
            }
        }

    private:
        struct FQueueEntry {
            u64 mEnd = 0ULL;
            u64 mTag = 0ULL;
        };

        [[nodiscard]] static auto AlignUp(u64 value, u64 alignment) noexcept -> u64 {
            if (alignment == 0ULL) {
                return value;
            }
            const u64 remainder = value % alignment;
            return (remainder == 0ULL) ? value : (value + (alignment - remainder));
        }

        auto CommitAllocation(u64 offset, u64 sizeBytes, u64 tag) -> FRingAllocation {
            const u64 end = offset + sizeBytes;
            mHead         = (end >= mCapacity) ? 0ULL : end;
            mQueue.PushBack(FQueueEntry{ end, tag });

            FRingAllocation allocation{};
            allocation.mOffset = offset;
            allocation.mSize   = sizeBytes;
            allocation.mTag    = tag;
            return allocation;
        }

        u64                 mCapacity = 0ULL;
        u64                 mHead     = 0ULL;
        u64                 mTail     = 0ULL;
        TDeque<FQueueEntry> mQueue;
    };
} // namespace AltinaEngine::Core::Memory
