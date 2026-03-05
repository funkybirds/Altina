#ifndef ALTINAENGINE_CORE_PUBLIC_CONTAINER_ROBINHOODHASHMAP_H
#define ALTINAENGINE_CORE_PUBLIC_CONTAINER_ROBINHOODHASHMAP_H

#include "../Types/Aliases.h"
#include "../Types/Traits.h"
#include "Allocator.h"
#include "HashUtility.h"
#include "Pair.h"

namespace AltinaEngine::Core::Container {

    /**
     * Open-addressing hash map using robin-hood probing and backward-shift erase.
     * This container exposes a compact unordered_map-like API for engine migration.
     */
    template <typename TKey, typename TValue, typename THasher = THashFunc<TKey>,
        typename TKeyEqual = TEqual<TKey>>
    class TRobinHoodHashMap {
    private:
        struct FBucketMeta;

    public:
        using key_type    = TKey;      // NOLINT(*-identifier-naming)
        using mapped_type = TValue;    // NOLINT(*-identifier-naming)
        using size_type   = usize;     // NOLINT(*-identifier-naming)
        using hasher      = THasher;   // NOLINT(*-identifier-naming)
        using key_equal   = TKeyEqual; // NOLINT(*-identifier-naming)

        struct value_type {
            TKey   first;
            TValue second;
        };

        class iterator {
        public:
            using value_type        = TRobinHoodHashMap::value_type; // NOLINT(*-identifier-naming)
            using difference_type   = isize;                         // NOLINT(*-identifier-naming)
            using pointer           = value_type*;                   // NOLINT(*-identifier-naming)
            using reference         = value_type&;                   // NOLINT(*-identifier-naming)
            using iterator_category = void;                          // NOLINT(*-identifier-naming)

            iterator() = default;
            iterator(TRobinHoodHashMap* owner, size_type index) : mOwner(owner), mIndex(index) {}

            auto operator*() const -> reference { return *mOwner->EntryAt(mIndex); }
            auto operator->() const -> pointer { return mOwner->EntryAt(mIndex); }

            auto operator++() -> iterator& {
                ++mIndex;
                SkipToOccupied();
                return *this;
            }
            auto operator++(int) -> iterator {
                iterator copy(*this);
                ++(*this);
                return copy;
            }

            [[nodiscard]] auto operator==(const iterator& rhs) const -> bool {
                return mOwner == rhs.mOwner && mIndex == rhs.mIndex;
            }
            [[nodiscard]] auto operator!=(const iterator& rhs) const -> bool {
                return !(*this == rhs);
            }

            [[nodiscard]] auto Index() const -> size_type { return mIndex; }

        private:
            void SkipToOccupied() {
                if (mOwner == nullptr) {
                    return;
                }
                while (mIndex < mOwner->mBucketCount && mOwner->mMetas[mIndex].Occupied == 0) {
                    ++mIndex;
                }
            }

            TRobinHoodHashMap* mOwner = nullptr;
            size_type          mIndex = 0;

            friend class TRobinHoodHashMap;
        };

        class const_iterator {
        public:
            using value_type = const TRobinHoodHashMap::value_type; // NOLINT(*-identifier-naming)
            using difference_type = isize;                          // NOLINT(*-identifier-naming)
            using pointer   = const TRobinHoodHashMap::value_type*; // NOLINT(*-identifier-naming)
            using reference = const TRobinHoodHashMap::value_type&; // NOLINT(*-identifier-naming)
            using iterator_category = void;                         // NOLINT(*-identifier-naming)

            const_iterator() = default;
            const_iterator(const TRobinHoodHashMap* owner, size_type index)
                : mOwner(owner), mIndex(index) {}
            const_iterator(iterator it) : mOwner(it.mOwner), mIndex(it.mIndex) {}

            auto operator*() const -> reference { return *mOwner->EntryAt(mIndex); }
            auto operator->() const -> pointer { return mOwner->EntryAt(mIndex); }

            auto operator++() -> const_iterator& {
                ++mIndex;
                SkipToOccupied();
                return *this;
            }
            auto operator++(int) -> const_iterator {
                const_iterator copy(*this);
                ++(*this);
                return copy;
            }

            [[nodiscard]] auto operator==(const const_iterator& rhs) const -> bool {
                return mOwner == rhs.mOwner && mIndex == rhs.mIndex;
            }
            [[nodiscard]] auto operator!=(const const_iterator& rhs) const -> bool {
                return !(*this == rhs);
            }

            [[nodiscard]] auto Index() const -> size_type { return mIndex; }

        private:
            void SkipToOccupied() {
                if (mOwner == nullptr) {
                    return;
                }
                while (mIndex < mOwner->mBucketCount && mOwner->mMetas[mIndex].Occupied == 0) {
                    ++mIndex;
                }
            }

            const TRobinHoodHashMap* mOwner = nullptr;
            size_type                mIndex = 0;

            friend class TRobinHoodHashMap;
        };

        using InsertResult = TPair<iterator, bool>;

        TRobinHoodHashMap() = default;
        explicit TRobinHoodHashMap(size_type initialCapacity) { Reserve(initialCapacity); }

        TRobinHoodHashMap(const TRobinHoodHashMap& other) { CopyFrom(other); }

        auto operator=(const TRobinHoodHashMap& other) -> TRobinHoodHashMap& {
            if (this != &other) {
                Clear();
                ReleaseStorage();
                CopyFrom(other);
            }
            return *this;
        }

        TRobinHoodHashMap(TRobinHoodHashMap&& other) noexcept { MoveFrom(Move(other)); }

        auto operator=(TRobinHoodHashMap&& other) noexcept -> TRobinHoodHashMap& {
            if (this != &other) {
                Clear();
                ReleaseStorage();
                MoveFrom(Move(other));
            }
            return *this;
        }

        ~TRobinHoodHashMap() {
            Clear();
            ReleaseStorage();
        }

        [[nodiscard]] auto begin() -> iterator {
            iterator it(this, 0);
            it.SkipToOccupied();
            return it;
        }
        [[nodiscard]] auto begin() const -> const_iterator {
            const_iterator it(this, 0);
            it.SkipToOccupied();
            return it;
        }
        [[nodiscard]] auto cbegin() const -> const_iterator { return begin(); }
        [[nodiscard]] auto end() -> iterator { return iterator(this, mBucketCount); }
        [[nodiscard]] auto end() const -> const_iterator {
            return const_iterator(this, mBucketCount);
        }
        [[nodiscard]] auto cend() const -> const_iterator { return end(); }

        [[nodiscard]] auto IsEmpty() const noexcept -> bool { return mSize == 0; }
        [[nodiscard]] auto Num() const noexcept -> size_type { return mSize; }

        [[nodiscard]] auto LoadFactor() const noexcept -> f32 {
            if (mBucketCount == 0) {
                return 0.0F;
            }
            return static_cast<f32>(mSize) / static_cast<f32>(mBucketCount);
        }

        [[nodiscard]] auto GetMaxLoadFactor() const noexcept -> f32 { return mMaxLoadFactor; }
        void               SetMaxLoadFactor(f32 value) noexcept {
            if (value < 0.10F) {
                mMaxLoadFactor = 0.10F;
            } else if (value > 0.95F) {
                mMaxLoadFactor = 0.95F;
            } else {
                mMaxLoadFactor = value;
            }
        }

        [[nodiscard]] auto FindIt(const TKey& key) -> iterator {
            const size_type index = FindIndex(key);
            if (index == kInvalidIndex) {
                return end();
            }
            return iterator(this, index);
        }

        [[nodiscard]] auto FindIt(const TKey& key) const -> const_iterator {
            const size_type index = FindIndex(key);
            if (index == kInvalidIndex) {
                return end();
            }
            return const_iterator(this, index);
        }

        [[nodiscard]] auto Find(const TKey& key) -> TValue* {
            iterator it = FindIt(key);
            if (it == end()) {
                return nullptr;
            }
            return &it->second;
        }
        [[nodiscard]] auto Find(const TKey& key) const -> const TValue* {
            const_iterator it = FindIt(key);
            if (it == end()) {
                return nullptr;
            }
            return &it->second;
        }

        [[nodiscard]] auto Count(const TKey& key) const -> size_type {
            return Contains(key) ? 1 : 0;
        }
        [[nodiscard]] auto Contains(const TKey& key) const -> bool {
            return FindIndex(key) != kInvalidIndex;
        }
        [[nodiscard]] auto HasKey(const TKey& key) const -> bool { return Contains(key); }

        auto               At(const TKey& key) -> TValue& {
            iterator it = FindIt(key);
            return it->second;
        }
        auto At(const TKey& key) const -> const TValue& {
            const_iterator it = FindIt(key);
            return it->second;
        }

        auto operator[](const TKey& key) -> TValue& { return TryEmplace(key).first->second; }
        auto operator[](TKey&& key) -> TValue& { return TryEmplace(Move(key)).first->second; }

        auto Insert(const value_type& value) -> InsertResult {
            return InsertImpl(value.first, value.second);
        }
        auto Insert(value_type&& value) -> InsertResult {
            return InsertImpl(Move(value.first), Move(value.second));
        }
        template <typename TKeyArg, typename TValueArg>
        auto Emplace(TKeyArg&& key, TValueArg&& value) -> InsertResult {
            return InsertImpl(Forward<TKeyArg>(key), Forward<TValueArg>(value));
        }
        template <typename... TArgs>
        auto TryEmplace(const TKey& key, TArgs&&... args) -> InsertResult {
            iterator existing = FindIt(key);
            if (existing != end()) {
                return { existing, false };
            }
            TValue value(Forward<TArgs>(args)...);
            return InsertImpl(key, Move(value));
        }
        template <typename... TArgs> auto TryEmplace(TKey&& key, TArgs&&... args) -> InsertResult {
            iterator existing = FindIt(key);
            if (existing != end()) {
                return { existing, false };
            }
            TValue value(Forward<TArgs>(args)...);
            return InsertImpl(Move(key), Move(value));
        }

        auto InsertOrAssign(const TKey& key, const TValue& value) -> InsertResult {
            iterator it = FindIt(key);
            if (it != end()) {
                it->second = value;
                return { it, false };
            }
            return InsertImpl(key, value);
        }
        auto InsertOrAssign(TKey&& key, TValue&& value) -> InsertResult {
            iterator it = FindIt(key);
            if (it != end()) {
                it->second = Move(value);
                return { it, false };
            }
            return InsertImpl(Move(key), Move(value));
        }

        auto Erase(const TKey& key) -> size_type { return Remove(key) ? 1 : 0; }
        auto Erase(iterator position) -> iterator {
            if (position == end()) {
                return end();
            }
            const size_type index = position.Index();
            RemoveAtIndex(index);
            return iterator(this, index);
        }
        auto Remove(const TKey& key) -> bool {
            if (mBucketCount == 0) {
                return false;
            }

            const size_type index = FindIndex(key);
            if (index == kInvalidIndex) {
                return false;
            }
            RemoveAtIndex(index);
            return true;
        }

        void Clear() noexcept {
            for (size_type i = 0; i < mBucketCount; ++i) {
                if (mMetas[i].Occupied != 0) {
                    DestroyEntry(i);
                    mMetas[i].Hash     = 0;
                    mMetas[i].Distance = 0;
                    mMetas[i].Occupied = 0;
                }
            }
            mSize = 0;
        }

        void Reserve(size_type expectedCount) {
            if (expectedCount == 0) {
                return;
            }
            size_type minBuckets =
                static_cast<size_type>(static_cast<f32>(expectedCount) / mMaxLoadFactor) + 1;
            if (minBuckets > mBucketCount) {
                Rehash(minBuckets);
            }
        }

        void Rehash(size_type requestedBuckets) {
            size_type newBucketCount = NormalizeBucketCount(requestedBuckets);
            if (newBucketCount < MinimumBucketsForSize(mSize)) {
                newBucketCount = MinimumBucketsForSize(mSize);
            }
            if (newBucketCount == mBucketCount) {
                return;
            }

            FBucketMeta*   newMetas   = mMetaAllocator.Allocate(newBucketCount);
            FEntryStorage* newStorage = mStorageAllocator.Allocate(newBucketCount);
            InitializeMetas(newMetas, newBucketCount);

            FBucketMeta*    oldMetas   = mMetas;
            FEntryStorage*  oldStorage = mStorage;
            const size_type oldBuckets = mBucketCount;

            mMetas       = newMetas;
            mStorage     = newStorage;
            mBucketCount = newBucketCount;
            mSize        = 0;

            for (size_type i = 0; i < oldBuckets; ++i) {
                if (oldMetas[i].Occupied != 0) {
                    value_type* entry = EntryAt(oldStorage, i);
                    InsertImpl(Move(entry->first), Move(entry->second));
                    entry->~value_type();
                }
            }

            if (oldMetas != nullptr) {
                mMetaAllocator.Deallocate(oldMetas, oldBuckets);
            }
            if (oldStorage != nullptr) {
                mStorageAllocator.Deallocate(oldStorage, oldBuckets);
            }
        }

    private:
        struct FBucketMeta {
            u64 Hash     = 0;
            u32 Distance = 0;
            u8  Occupied = 0;
        };

        struct FEntryStorage {
            alignas(value_type) u8 Bytes[sizeof(value_type)];
        };

        static constexpr size_type kDefaultBucketCount = 8;
        static constexpr size_type kInvalidIndex       = static_cast<size_type>(-1);

        using FMetaAllocator  = TAllocator<FBucketMeta>;
        using FStoreAllocator = TAllocator<FEntryStorage>;

        FBucketMeta*              mMetas         = nullptr;
        FEntryStorage*            mStorage       = nullptr;
        size_type                 mBucketCount   = 0;
        size_type                 mSize          = 0;
        f32                       mMaxLoadFactor = 0.85F;
        THasher                   mHasher{};
        TKeyEqual                 mKeyEqual{};
        FMetaAllocator            mMetaAllocator{};
        FStoreAllocator           mStorageAllocator{};

        [[nodiscard]] static auto NextPow2(size_type value) -> size_type {
            if (value <= 1) {
                return 1;
            }
            --value;
            value |= (value >> 1);
            value |= (value >> 2);
            value |= (value >> 4);
            value |= (value >> 8);
            value |= (value >> 16);
            if constexpr (sizeof(size_type) >= 8) {
                value |= (value >> 32);
            }
            return value + 1;
        }

        [[nodiscard]] auto NormalizeBucketCount(size_type requested) const -> size_type {
            if (requested == 0) {
                return kDefaultBucketCount;
            }
            size_type normalized = NextPow2(requested);
            if (normalized < kDefaultBucketCount) {
                normalized = kDefaultBucketCount;
            }
            return normalized;
        }

        [[nodiscard]] auto MinimumBucketsForSize(size_type elementCount) const -> size_type {
            if (elementCount == 0) {
                return kDefaultBucketCount;
            }
            const size_type minBuckets =
                static_cast<size_type>(static_cast<f32>(elementCount) / mMaxLoadFactor) + 1;
            return NormalizeBucketCount(minBuckets);
        }

        [[nodiscard]] auto MaskedIndex(size_type index) const -> size_type {
            return index & (mBucketCount - 1);
        }

        [[nodiscard]] auto NonZeroHash(const TKey& key) const -> u64 {
            u64 h = static_cast<u64>(mHasher(key));
            if (h == 0) {
                h = 1;
            }
            return h;
        }

        void EnsureCapacityForOneMore() {
            if (mBucketCount == 0) {
                Rehash(kDefaultBucketCount);
                return;
            }

            const f32 nextLoad = static_cast<f32>(mSize + 1) / static_cast<f32>(mBucketCount);
            if (nextLoad > mMaxLoadFactor) {
                Rehash(mBucketCount * 2);
            }
        }

        template <typename TKeyArg, typename TValueArg>
        auto InsertImpl(TKeyArg&& key, TValueArg&& value) -> InsertResult {
            EnsureCapacityForOneMore();

            u64       candidateHash = NonZeroHash(key);
            size_type index         = MaskedIndex(static_cast<size_type>(candidateHash));
            u32       distance      = 0;
            TKey      candidateKey(Forward<TKeyArg>(key));
            TValue    candidateValue(Forward<TValueArg>(value));
            size_type insertedIndex = kInvalidIndex;

            while (true) {
                FBucketMeta& meta = mMetas[index];
                if (meta.Occupied == 0) {
                    ConstructEntry(index, Move(candidateKey), Move(candidateValue));
                    meta.Hash     = candidateHash;
                    meta.Distance = distance;
                    meta.Occupied = 1;
                    ++mSize;
                    if (insertedIndex == kInvalidIndex) {
                        insertedIndex = index;
                    }
                    return { iterator(this, insertedIndex), true };
                }

                value_type* entry = EntryAt(index);
                if (meta.Hash == candidateHash && mKeyEqual(entry->first, candidateKey)) {
                    return { iterator(this, index), false };
                }

                if (meta.Distance < distance) {
                    if (insertedIndex == kInvalidIndex) {
                        // The original input key/value is placed into this bucket at first swap.
                        insertedIndex = index;
                    }
                    const u64 oldHash     = meta.Hash;
                    const u32 oldDistance = meta.Distance;
                    meta.Hash             = candidateHash;
                    meta.Distance         = distance;
                    candidateHash         = oldHash;
                    distance              = oldDistance;

                    TKey oldKey(Move(entry->first));
                    entry->first = Move(candidateKey);
                    candidateKey = Move(oldKey);

                    TValue oldValue(Move(entry->second));
                    entry->second  = Move(candidateValue);
                    candidateValue = Move(oldValue);
                }

                ++distance;
                index = MaskedIndex(index + 1);
            }
        }

        [[nodiscard]] auto FindIndex(const TKey& key) const -> size_type {
            if (mBucketCount == 0) {
                return kInvalidIndex;
            }

            const u64 hash  = NonZeroHash(key);
            size_type index = MaskedIndex(static_cast<size_type>(hash));

            while (true) {
                const FBucketMeta& meta = mMetas[index];
                if (meta.Occupied == 0) {
                    return kInvalidIndex;
                }

                const value_type* entry = EntryAt(index);
                if (meta.Hash == hash && mKeyEqual(entry->first, key)) {
                    return index;
                }

                index = MaskedIndex(index + 1);
            }
        }

        static void InitializeMetas(FBucketMeta* metas, size_type bucketCount) {
            for (size_type i = 0; i < bucketCount; ++i) {
                metas[i].Hash     = 0;
                metas[i].Distance = 0;
                metas[i].Occupied = 0;
            }
        }

        void ConstructEntry(size_type index, TKey&& key, TValue&& value) {
            void* raw = static_cast<void*>(mStorage[index].Bytes);
            ::new (raw) value_type{ Move(key), Move(value) };
        }

        void DestroyEntry(size_type index) noexcept { EntryAt(index)->~value_type(); }

        void MoveEntry(size_type src, size_type dst) {
            value_type* srcEntry = EntryAt(src);
            void*       dstRaw   = static_cast<void*>(mStorage[dst].Bytes);
            ::new (dstRaw) value_type{ Move(srcEntry->first), Move(srcEntry->second) };
            srcEntry->~value_type();
        }

        void RemoveAtIndex(size_type index) {
            DestroyEntry(index);
            mMetas[index].Occupied = 0;

            size_type hole = index;
            size_type next = MaskedIndex(index + 1);
            while (mMetas[next].Occupied != 0 && mMetas[next].Distance > 0) {
                MoveEntry(next, hole);
                mMetas[hole].Hash     = mMetas[next].Hash;
                mMetas[hole].Distance = mMetas[next].Distance - 1;
                mMetas[hole].Occupied = 1;

                mMetas[next].Hash     = 0;
                mMetas[next].Distance = 0;
                mMetas[next].Occupied = 0;

                hole = next;
                next = MaskedIndex(next + 1);
            }

            mMetas[hole].Hash     = 0;
            mMetas[hole].Distance = 0;
            mMetas[hole].Occupied = 0;
            --mSize;
        }

        [[nodiscard]] auto EntryAt(size_type index) -> value_type* {
            return reinterpret_cast<value_type*>(mStorage[index].Bytes);
        }
        [[nodiscard]] auto EntryAt(size_type index) const -> const value_type* {
            return reinterpret_cast<const value_type*>(mStorage[index].Bytes);
        }
        [[nodiscard]] static auto EntryAt(FEntryStorage* storage, size_type index) -> value_type* {
            return reinterpret_cast<value_type*>(storage[index].Bytes);
        }

        void ReleaseStorage() noexcept {
            if (mMetas != nullptr) {
                mMetaAllocator.Deallocate(mMetas, mBucketCount);
                mMetas = nullptr;
            }
            if (mStorage != nullptr) {
                mStorageAllocator.Deallocate(mStorage, mBucketCount);
                mStorage = nullptr;
            }
            mBucketCount = 0;
        }

        void MoveFrom(TRobinHoodHashMap&& other) noexcept {
            mMetas         = other.mMetas;
            mStorage       = other.mStorage;
            mBucketCount   = other.mBucketCount;
            mSize          = other.mSize;
            mMaxLoadFactor = other.mMaxLoadFactor;
            mHasher        = Move(other.mHasher);
            mKeyEqual      = Move(other.mKeyEqual);

            other.mMetas       = nullptr;
            other.mStorage     = nullptr;
            other.mBucketCount = 0;
            other.mSize        = 0;
        }

        void CopyFrom(const TRobinHoodHashMap& other) {
            mMaxLoadFactor = other.mMaxLoadFactor;
            mHasher        = other.mHasher;
            mKeyEqual      = other.mKeyEqual;
            if (other.mBucketCount == 0) {
                return;
            }

            Rehash(other.mBucketCount);
            for (size_type i = 0; i < other.mBucketCount; ++i) {
                if (other.mMetas[i].Occupied != 0) {
                    const value_type* src = other.EntryAt(i);
                    InsertImpl(src->first, src->second);
                }
            }
        }
    };

} // namespace AltinaEngine::Core::Container

#endif // ALTINAENGINE_CORE_PUBLIC_CONTAINER_ROBINHOODHASHMAP_H
