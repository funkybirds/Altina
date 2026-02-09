#pragma once

#include "Container/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Core::Memory {
    struct FBuddyAllocation {
        u64 mOffset = 0ULL;
        u64 mSize   = 0ULL;
        u32 mOrder  = 0U;

        [[nodiscard]] auto IsValid() const noexcept -> bool { return mSize != 0ULL; }
    };

    class FBuddyAllocatorPolicy {
    public:
        FBuddyAllocatorPolicy() = default;
        FBuddyAllocatorPolicy(u64 totalSizeBytes, u64 minBlockSizeBytes) {
            Init(totalSizeBytes, minBlockSizeBytes);
        }

        void Init(u64 totalSizeBytes, u64 minBlockSizeBytes) {
            mMinBlockSize = NormalizeMinBlockSize(minBlockSizeBytes);
            if (mMinBlockSize == 0ULL || totalSizeBytes == 0ULL) {
                mTotalSize = 0ULL;
                mMaxOrder  = 0U;
                mFreeLists.Clear();
                return;
            }
            mTotalSize    = NextPowerOfTwo(Max(totalSizeBytes, mMinBlockSize));
            mMaxOrder     = ComputeOrder(mTotalSize);

            mFreeLists.Clear();
            mFreeLists.Resize(static_cast<usize>(mMaxOrder) + 1U);
            mFreeLists[static_cast<usize>(mMaxOrder)].PushBack(0ULL);
        }

        void Reset() {
            mFreeLists.Clear();
            if (mMinBlockSize == 0ULL || mTotalSize == 0ULL) {
                return;
            }
            mFreeLists.Resize(static_cast<usize>(mMaxOrder) + 1U);
            mFreeLists[static_cast<usize>(mMaxOrder)].PushBack(0ULL);
        }

        [[nodiscard]] auto GetTotalSize() const noexcept -> u64 { return mTotalSize; }
        [[nodiscard]] auto GetMinBlockSize() const noexcept -> u64 { return mMinBlockSize; }
        [[nodiscard]] auto GetMaxOrder() const noexcept -> u32 { return mMaxOrder; }

        auto Allocate(u64 sizeBytes, u64 alignment) -> FBuddyAllocation {
            if (sizeBytes == 0ULL || mMinBlockSize == 0ULL || mTotalSize == 0ULL) {
                return {};
            }

            const u64 align = NormalizeAlignment(alignment);
            u64       required = Max(sizeBytes, align);
            required = NextPowerOfTwo(Max(required, mMinBlockSize));

            const u32 targetOrder = ComputeOrder(required);
            if (targetOrder > mMaxOrder) {
                return {};
            }

            u32 order = targetOrder;
            while (order <= mMaxOrder && mFreeLists[static_cast<usize>(order)].IsEmpty()) {
                ++order;
            }
            if (order > mMaxOrder) {
                return {};
            }

            u64 offset = PopFreeBlock(order);
            while (order > targetOrder) {
                --order;
                const u64 splitSize = BlockSize(order);
                const u64 buddyOffset = offset + splitSize;
                mFreeLists[static_cast<usize>(order)].PushBack(buddyOffset);
            }

            FBuddyAllocation allocation{};
            allocation.mOffset = offset;
            allocation.mSize   = BlockSize(order);
            allocation.mOrder  = order;
            return allocation;
        }

        auto Free(const FBuddyAllocation& allocation) -> bool {
            if (!allocation.IsValid() || allocation.mOrder > mMaxOrder) {
                return false;
            }

            u64 offset = allocation.mOffset;
            u32 order  = allocation.mOrder;

            while (order < mMaxOrder) {
                const u64 buddyOffset = offset ^ BlockSize(order);
                if (!TryRemoveFreeBlock(order, buddyOffset)) {
                    break;
                }
                offset = Min(offset, buddyOffset);
                ++order;
            }

            mFreeLists[static_cast<usize>(order)].PushBack(offset);
            return true;
        }

    private:
        [[nodiscard]] static auto NextPowerOfTwo(u64 value) noexcept -> u64 {
            if (value <= 1ULL) {
                return 1ULL;
            }
            value--;
            value |= value >> 1ULL;
            value |= value >> 2ULL;
            value |= value >> 4ULL;
            value |= value >> 8ULL;
            value |= value >> 16ULL;
            value |= value >> 32ULL;
            return value + 1ULL;
        }

        [[nodiscard]] static auto NormalizeAlignment(u64 alignment) noexcept -> u64 {
            if (alignment == 0ULL) {
                return 1ULL;
            }
            return NextPowerOfTwo(alignment);
        }

        [[nodiscard]] static auto NormalizeMinBlockSize(u64 minBlockSizeBytes) noexcept -> u64 {
            if (minBlockSizeBytes == 0ULL) {
                return 0ULL;
            }
            return NextPowerOfTwo(minBlockSizeBytes);
        }

        [[nodiscard]] auto ComputeOrder(u64 sizeBytes) const noexcept -> u32 {
            u32 order = 0U;
            u64 size  = mMinBlockSize;
            while (size < sizeBytes) {
                size <<= 1ULL;
                ++order;
            }
            return order;
        }

        [[nodiscard]] auto BlockSize(u32 order) const noexcept -> u64 {
            return mMinBlockSize << order;
        }

        auto PopFreeBlock(u32 order) -> u64 {
            auto& list = mFreeLists[static_cast<usize>(order)];
            const u64 offset = list.Back();
            list.PopBack();
            return offset;
        }

        auto TryRemoveFreeBlock(u32 order, u64 offset) -> bool {
            auto& list = mFreeLists[static_cast<usize>(order)];
            for (usize i = 0; i < list.Size(); ++i) {
                if (list[i] == offset) {
                    list[i] = list.Back();
                    list.PopBack();
                    return true;
                }
            }
            return false;
        }

        u64 mTotalSize    = 0ULL;
        u64 mMinBlockSize = 0ULL;
        u32 mMaxOrder     = 0U;
        Core::Container::TVector<Core::Container::TVector<u64>> mFreeLists;

        [[nodiscard]] static constexpr auto Max(u64 a, u64 b) noexcept -> u64 {
            return (a > b) ? a : b;
        }

        [[nodiscard]] static constexpr auto Min(u64 a, u64 b) noexcept -> u64 {
            return (a < b) ? a : b;
        }
    };
} // namespace AltinaEngine::Core::Memory
