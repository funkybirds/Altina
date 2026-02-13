#pragma once

#include "Platform/Generic/GenericPlatformDecl.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"

namespace AltinaEngine::Core::Memory {
    struct FMemoryBufferBacking {
        u8* mData      = nullptr;
        u64 mSizeBytes = 0ULL;

        FMemoryBufferBacking() = default;
        FMemoryBufferBacking(void* data, u64 sizeBytes)
            : mData(static_cast<u8*>(data)), mSizeBytes(sizeBytes) {}

        [[nodiscard]] auto IsValid() const noexcept -> bool {
            return (mData != nullptr) && (mSizeBytes != 0ULL);
        }

        [[nodiscard]] auto GetData() noexcept -> void* { return mData; }
        [[nodiscard]] auto GetData() const noexcept -> const void* { return mData; }
        [[nodiscard]] auto GetSizeBytes() const noexcept -> u64 { return mSizeBytes; }
    };

    namespace Detail {
        using AltinaEngine::CStaticConvertible;

        template <typename T>
        concept CBufferBackingHasSize = requires(const T& b) {
            { b.GetSizeBytes() } -> CStaticConvertible<u64>;
        };

        template <typename T>
        concept CBufferBackingHasWrite = requires(T& b, u64 offset, const void* data, u64 size) {
            { b.Write(offset, data, size) } -> CStaticConvertible<bool>;
        };

        template <typename T>
        concept CBufferBackingHasData = requires(T& b) {
            { b.GetData() } -> CStaticConvertible<void*>;
        };

        template <typename T>
        concept CAllocatorAllocation = requires(const T& a) {
            { a.IsValid() } -> CStaticConvertible<bool>;
            { a.mOffset } -> CStaticConvertible<u64>;
            { a.mSize } -> CStaticConvertible<u64>;
        };
    } // namespace Detail

    template <typename TPolicy, typename TBacking> class TAllocatorExecutor {
    public:
        using FPolicy  = TPolicy;
        using FBacking = TBacking;

        TAllocatorExecutor() = default;
        explicit TAllocatorExecutor(const FBacking& backing) : mBacking(backing) {}

        void               SetBacking(const FBacking& backing) { mBacking = backing; }

        [[nodiscard]] auto GetBacking() noexcept -> FBacking& { return mBacking; }
        [[nodiscard]] auto GetBacking() const noexcept -> const FBacking& { return mBacking; }

        [[nodiscard]] auto GetPolicy() noexcept -> FPolicy& { return mPolicy; }
        [[nodiscard]] auto GetPolicy() const noexcept -> const FPolicy& { return mPolicy; }

        void               Reset() {
            if constexpr (requires(FPolicy& p) { p.Reset(); }) {
                mPolicy.Reset();
            }
        }

        void InitPolicyFromBacking()
            requires Detail::CBufferBackingHasSize<FBacking>
            && requires(FPolicy& p, u64 sizeBytes) { p.Init(sizeBytes); }
        {
            mPolicy.Init(mBacking.GetSizeBytes());
        }

        void InitPolicyFromBacking(u64 minBlockSizeBytes)
            requires Detail::CBufferBackingHasSize<FBacking>
            && requires(
                FPolicy& p, u64 sizeBytes, u64 minBlockBytes) { p.Init(sizeBytes, minBlockBytes); }
        {
            mPolicy.Init(mBacking.GetSizeBytes(), minBlockSizeBytes);
        }

        template <typename... Args> auto Allocate(Args&&... args) {
            return mPolicy.Allocate(AltinaEngine::Forward<Args>(args)...);
        }

        template <typename TAllocation>
            requires Detail::CAllocatorAllocation<TAllocation>
        auto Write(const TAllocation& allocation, const void* data, u64 sizeBytes,
            u64 dstOffset = 0ULL) -> bool {
            if (!allocation.IsValid() || data == nullptr || sizeBytes == 0ULL) {
                return false;
            }
            if (dstOffset > allocation.mSize || sizeBytes > (allocation.mSize - dstOffset)) {
                return false;
            }
            const u64 writeOffset = allocation.mOffset + dstOffset;
            if (writeOffset < allocation.mOffset) {
                return false;
            }

            if constexpr (Detail::CBufferBackingHasSize<FBacking>) {
                const u64 backingSize = mBacking.GetSizeBytes();
                if (writeOffset > backingSize || sizeBytes > (backingSize - writeOffset)) {
                    return false;
                }
            }

            if constexpr (Detail::CBufferBackingHasWrite<FBacking>) {
                return mBacking.Write(writeOffset, data, sizeBytes);
            } else if constexpr (Detail::CBufferBackingHasData<FBacking>) {
                auto* dst = static_cast<u8*>(mBacking.GetData());
                if (dst == nullptr) {
                    return false;
                }
                Platform::Generic::Memcpy(dst + writeOffset, data, static_cast<usize>(sizeBytes));
                return true;
            } else {
                return false;
            }
        }

        template <typename TAllocation>
            requires Detail::CAllocatorAllocation<TAllocation>
            && Detail::CBufferBackingHasData<FBacking>
        [[nodiscard]] auto GetWritePointer(const TAllocation& allocation, u64 dstOffset = 0ULL)
            -> u8* {
            if (!allocation.IsValid()) {
                return nullptr;
            }
            if (dstOffset >= allocation.mSize) {
                return nullptr;
            }
            const u64 writeOffset = allocation.mOffset + dstOffset;
            if (writeOffset < allocation.mOffset) {
                return nullptr;
            }

            if constexpr (Detail::CBufferBackingHasSize<FBacking>) {
                const u64 backingSize = mBacking.GetSizeBytes();
                if (writeOffset >= backingSize) {
                    return nullptr;
                }
            }

            auto* dst = static_cast<u8*>(mBacking.GetData());
            return (dst == nullptr) ? nullptr : (dst + writeOffset);
        }

        void ReleaseUpTo(u64 tag)
            requires requires(FPolicy& p) { p.ReleaseUpTo(tag); }
        {
            mPolicy.ReleaseUpTo(tag);
        }

        template <typename TAllocation>
        auto Free(const TAllocation& allocation) -> bool
            requires requires(FPolicy& p, const TAllocation& a) { p.Free(a); }
        {
            return mPolicy.Free(allocation);
        }

    private:
        FPolicy  mPolicy{};
        FBacking mBacking{};
    };
} // namespace AltinaEngine::Core::Memory
