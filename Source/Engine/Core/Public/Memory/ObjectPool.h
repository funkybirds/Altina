#pragma once

#include "Container/Allocator.h"
#include "Threading/Mutex.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"

#include <new>

namespace AltinaEngine::Core::Memory {
    namespace Container = Core::Container;
    namespace Detail {
        [[nodiscard]] constexpr auto DivRoundUp(usize numerator, usize denominator) noexcept
            -> usize {
            return (denominator == 0) ? 0 : ((numerator + denominator - 1) / denominator);
        }
    } // namespace Detail

    template <typename T> class TObjectPoolHandle {
    public:
        using TValueType = T;

        TObjectPoolHandle()                                            = default;
        TObjectPoolHandle(const TObjectPoolHandle&)                    = delete;
        auto operator=(const TObjectPoolHandle&) -> TObjectPoolHandle& = delete;

        TObjectPoolHandle(TObjectPoolHandle&& other) noexcept : mPtr(other.mPtr) {
            other.mPtr = nullptr;
        }
        auto operator=(TObjectPoolHandle&& other) noexcept -> TObjectPoolHandle& {
            if (this != &other) {
                mPtr       = other.mPtr;
                other.mPtr = nullptr;
            }
            return *this;
        }

        [[nodiscard]] auto     Get() noexcept -> T* { return mPtr; }
        [[nodiscard]] auto     Get() const noexcept -> const T* { return mPtr; }

        [[nodiscard]] auto     operator->() noexcept -> T* { return mPtr; }
        [[nodiscard]] auto     operator->() const noexcept -> const T* { return mPtr; }
        [[nodiscard]] auto     operator*() noexcept -> T& { return *mPtr; }
        [[nodiscard]] auto     operator*() const noexcept -> const T& { return *mPtr; }

        [[nodiscard]] explicit operator bool() const noexcept { return mPtr != nullptr; }

        void                   Reset() noexcept { mPtr = nullptr; }

        [[nodiscard]] auto     Release() noexcept -> T* {
            T* ptr = mPtr;
            mPtr   = nullptr;
            return ptr;
        }

    private:
        explicit TObjectPoolHandle(T* ptr) : mPtr(ptr) {}

        T* mPtr = nullptr;

        template <typename, template <typename, typename> class, typename> friend class TObjectPool;
    };

    template <typename T, typename TAllocator = Container::TAllocator<T>>
    class TSingleThreadedObjectPoolPolicy {
    public:
        using TValueType = T;
        using FAllocator = TAllocator;

        TSingleThreadedObjectPoolPolicy() = default;
        explicit TSingleThreadedObjectPoolPolicy(const FAllocator& allocator)
            : mChunkAllocator(allocator) {}

        TSingleThreadedObjectPoolPolicy(const TSingleThreadedObjectPoolPolicy&) = delete;
        auto operator=(const TSingleThreadedObjectPoolPolicy&)
            -> TSingleThreadedObjectPoolPolicy&                            = delete;
        TSingleThreadedObjectPoolPolicy(TSingleThreadedObjectPoolPolicy&&) = delete;
        auto operator=(TSingleThreadedObjectPoolPolicy&&)
            -> TSingleThreadedObjectPoolPolicy& = delete;

        [[nodiscard]] auto Allocate() -> T* {
            if (mFreeList == nullptr) {
                if (!AddChunk()) {
                    return nullptr;
                }
            }

            FNode* node = mFreeList;
            mFreeList   = node->mNext;
            return StorageFromNode(node);
        }

        void Deallocate(T* ptr) {
            if (ptr == nullptr) {
                return;
            }

            FNode* node = NodeFromStorage(ptr);
            node->mNext = mFreeList;
            mFreeList   = node;
        }

        void Initialize(usize size) {
            if (size == 0) {
                return;
            }

            const usize chunkCount = Detail::DivRoundUp(size, kElementsPerChunk);
            for (usize i = 0; i < chunkCount; ++i) {
                if (!AddChunk()) {
                    break;
                }
            }
        }

        void CleanUp() {
            FChunk* chunk = mChunkList;
            while (chunk != nullptr) {
                FChunk* next = chunk->mNext;
                FChunkAllocatorTraits::Destroy(mChunkAllocator, chunk);
                FChunkAllocatorTraits::Deallocate(mChunkAllocator, chunk, 1);
                chunk = next;
            }

            mChunkList           = nullptr;
            mFreeList            = nullptr;
            mAllocatedChunkCount = 0;
        }

        [[nodiscard]] auto GetPoolSize() const noexcept -> usize {
            return mAllocatedChunkCount * kElementsPerChunk;
        }

    private:
        union FNode {
            FNode* mNext;
            alignas(T) unsigned char mStorage[sizeof(T)];
        };

        static constexpr usize kChunkBytes       = 16 * 1024;
        static constexpr usize kElementsPerChunk = (kChunkBytes / sizeof(FNode)) > 0
            ? (kChunkBytes / sizeof(FNode))
            : 1;

        struct FChunk {
            FChunk* mNext = nullptr;
            FNode   mNodes[kElementsPerChunk];
        };

        using FChunkAllocator       = typename FAllocator::template Rebind<FChunk>::TOther;
        using FChunkAllocatorTraits = Container::TAllocatorTraits<FChunkAllocator>;

        [[nodiscard]] auto AddChunk() -> bool {
            FChunk* chunk = FChunkAllocatorTraits::Allocate(mChunkAllocator, 1);
            if (chunk == nullptr) {
                return false;
            }

            FChunkAllocatorTraits::Construct(mChunkAllocator, chunk);
            chunk->mNext = mChunkList;
            mChunkList   = chunk;
            ++mAllocatedChunkCount;

            for (usize i = 0; i < kElementsPerChunk; ++i) {
                FNode* node = &chunk->mNodes[i];
                node->mNext = mFreeList;
                mFreeList   = node;
            }

            return true;
        }

        [[nodiscard]] static auto StorageFromNode(FNode* node) noexcept -> T* {
            return reinterpret_cast<T*>(node);
        }

        [[nodiscard]] static auto NodeFromStorage(T* storage) noexcept -> FNode* {
            return reinterpret_cast<FNode*>(storage);
        }

        FNode*          mFreeList            = nullptr;
        FChunk*         mChunkList           = nullptr;
        usize           mAllocatedChunkCount = 0;
        FChunkAllocator mChunkAllocator{};
    };

    template <typename T, typename TAllocator = Container::TAllocator<T>>
    class TThreadSafeObjectPoolPolicy {
    public:
        using TValueType   = T;
        using FAllocator   = TAllocator;
        using FInnerPolicy = TSingleThreadedObjectPoolPolicy<T, FAllocator>;

        TThreadSafeObjectPoolPolicy() = default;
        explicit TThreadSafeObjectPoolPolicy(const FAllocator& allocator) : mPolicy(allocator) {}

        TThreadSafeObjectPoolPolicy(const TThreadSafeObjectPoolPolicy&)                    = delete;
        auto operator=(const TThreadSafeObjectPoolPolicy&) -> TThreadSafeObjectPoolPolicy& = delete;
        TThreadSafeObjectPoolPolicy(TThreadSafeObjectPoolPolicy&&)                         = delete;
        auto operator=(TThreadSafeObjectPoolPolicy&&) -> TThreadSafeObjectPoolPolicy&      = delete;

        [[nodiscard]] auto Allocate() -> T* {
            Threading::FScopedLock lock(mMutex);
            return mPolicy.Allocate();
        }

        void Deallocate(T* ptr) {
            Threading::FScopedLock lock(mMutex);
            mPolicy.Deallocate(ptr);
        }

        void Initialize(usize size) {
            Threading::FScopedLock lock(mMutex);
            mPolicy.Initialize(size);
        }

        void CleanUp() {
            Threading::FScopedLock lock(mMutex);
            mPolicy.CleanUp();
        }

        [[nodiscard]] auto GetPoolSize() const noexcept -> usize {
            Threading::FScopedLock lock(mMutex);
            return mPolicy.GetPoolSize();
        }

    private:
        mutable Threading::FMutex mMutex;
        FInnerPolicy              mPolicy;
    };

    template <typename T, template <typename, typename> class TPolicy = TThreadSafeObjectPoolPolicy,
        typename TAllocator = Container::TAllocator<T>>
    class TObjectPool {
    public:
        using TValueType = T;
        using FAllocator = TAllocator;
        using FPolicy    = TPolicy<T, FAllocator>;
        using FHandle    = TObjectPoolHandle<T>;

        TObjectPool() = default;
        explicit TObjectPool(const FAllocator& allocator) : mPolicy(allocator) {}
        ~TObjectPool() { mPolicy.CleanUp(); }

        TObjectPool(const TObjectPool&)                    = delete;
        auto operator=(const TObjectPool&) -> TObjectPool& = delete;
        TObjectPool(TObjectPool&&)                         = delete;
        auto operator=(TObjectPool&&) -> TObjectPool&      = delete;

        void Init(usize size) { mPolicy.Initialize(size); }

        template <typename... Args> [[nodiscard]] auto Allocate(Args&&... args) -> FHandle {
            T* mem = mPolicy.Allocate();
            if (mem == nullptr) {
                return {};
            }

            T* obj = new (mem) T(AltinaEngine::Forward<Args>(args)...);
            return FHandle(obj);
        }

        void               Deallocate(FHandle& handle) { DestroyRaw(handle.Release()); }
        void               Deallocate(FHandle&& handle) { DestroyRaw(handle.Release()); }

        [[nodiscard]] auto GetPoolSize() const noexcept -> usize { return mPolicy.GetPoolSize(); }

        [[nodiscard]] auto GetPolicy() noexcept -> FPolicy& { return mPolicy; }
        [[nodiscard]] auto GetPolicy() const noexcept -> const FPolicy& { return mPolicy; }

    private:
        void DestroyRaw(T* obj) {
            if (obj == nullptr) {
                return;
            }

            obj->~T();
            mPolicy.Deallocate(obj);
        }

        FPolicy mPolicy{};
    };

    template <typename T, typename TAllocator = Container::TAllocator<T>>
    using TThreadSafeObjectPool = TObjectPool<T, TThreadSafeObjectPoolPolicy, TAllocator>;

    template <typename T, typename TAllocator = Container::TAllocator<T>>
    using TSingleThreadedObjectPool = TObjectPool<T, TSingleThreadedObjectPoolPolicy, TAllocator>;
} // namespace AltinaEngine::Core::Memory
