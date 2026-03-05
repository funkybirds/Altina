#ifndef ALTINAENGINE_CORE_PUBLIC_CONTAINER_ROBINHOODHASHSET_H
#define ALTINAENGINE_CORE_PUBLIC_CONTAINER_ROBINHOODHASHSET_H

#include "HashUtility.h"
#include "RobinHoodHashMap.h"

namespace AltinaEngine::Core::Container {

    struct FHashSetPlaceholderValue {};

    template <typename TKey, typename THasher = THashFunc<TKey>, typename TKeyEqual = TEqual<TKey>>
    class TRobinHoodHashSet {
    private:
        using FMapType = TRobinHoodHashMap<TKey, FHashSetPlaceholderValue, THasher, TKeyEqual>;

    public:
        using key_type  = TKey;  // NOLINT(*-identifier-naming)
        using size_type = usize; // NOLINT(*-identifier-naming)

        class iterator {
        public:
            using value_type        = const TKey;  // NOLINT(*-identifier-naming)
            using difference_type   = isize;       // NOLINT(*-identifier-naming)
            using pointer           = const TKey*; // NOLINT(*-identifier-naming)
            using reference         = const TKey&; // NOLINT(*-identifier-naming)
            using iterator_category = void;        // NOLINT(*-identifier-naming)

            iterator() = default;
            explicit iterator(typename FMapType::iterator it) : mIt(it) {}

            auto operator*() const -> reference { return mIt->first; }
            auto operator->() const -> pointer { return &mIt->first; }

            auto operator++() -> iterator& {
                ++mIt;
                return *this;
            }
            auto operator++(int) -> iterator {
                iterator copy(*this);
                ++(*this);
                return copy;
            }

            [[nodiscard]] auto operator==(const iterator& rhs) const -> bool {
                return mIt == rhs.mIt;
            }
            [[nodiscard]] auto operator!=(const iterator& rhs) const -> bool {
                return !(*this == rhs);
            }

            [[nodiscard]] auto MapIterator() const -> const typename FMapType::iterator& {
                return mIt;
            }

        private:
            typename FMapType::iterator mIt{};
        };

        class const_iterator {
        public:
            using value_type        = const TKey;  // NOLINT(*-identifier-naming)
            using difference_type   = isize;       // NOLINT(*-identifier-naming)
            using pointer           = const TKey*; // NOLINT(*-identifier-naming)
            using reference         = const TKey&; // NOLINT(*-identifier-naming)
            using iterator_category = void;        // NOLINT(*-identifier-naming)

            const_iterator() = default;
            explicit const_iterator(typename FMapType::const_iterator it) : mIt(it) {}
            const_iterator(iterator it) : mIt(it.MapIterator()) {}

            auto operator*() const -> reference { return mIt->first; }
            auto operator->() const -> pointer { return &mIt->first; }

            auto operator++() -> const_iterator& {
                ++mIt;
                return *this;
            }
            auto operator++(int) -> const_iterator {
                const_iterator copy(*this);
                ++(*this);
                return copy;
            }

            [[nodiscard]] auto operator==(const const_iterator& rhs) const -> bool {
                return mIt == rhs.mIt;
            }
            [[nodiscard]] auto operator!=(const const_iterator& rhs) const -> bool {
                return !(*this == rhs);
            }

            [[nodiscard]] auto MapIterator() const -> const typename FMapType::const_iterator& {
                return mIt;
            }

        private:
            typename FMapType::const_iterator mIt{};
        };

        TRobinHoodHashSet() = default;
        explicit TRobinHoodHashSet(size_type capacity) : mMap(capacity) {}

        [[nodiscard]] auto begin() -> iterator { return iterator(mMap.begin()); }
        [[nodiscard]] auto begin() const -> const_iterator { return const_iterator(mMap.begin()); }
        [[nodiscard]] auto cbegin() const -> const_iterator { return begin(); }
        [[nodiscard]] auto end() -> iterator { return iterator(mMap.end()); }
        [[nodiscard]] auto end() const -> const_iterator { return const_iterator(mMap.end()); }
        [[nodiscard]] auto cend() const -> const_iterator { return end(); }

        [[nodiscard]] auto IsEmpty() const noexcept -> bool { return mMap.IsEmpty(); }
        [[nodiscard]] auto Num() const noexcept -> size_type { return mMap.Num(); }

        [[nodiscard]] auto Contains(const TKey& key) const -> bool { return mMap.Contains(key); }
        [[nodiscard]] auto HasKey(const TKey& key) const -> bool { return mMap.HasKey(key); }
        [[nodiscard]] auto Count(const TKey& key) const -> size_type { return mMap.Count(key); }

        [[nodiscard]] auto FindIt(const TKey& key) -> iterator {
            return iterator(mMap.FindIt(key));
        }
        [[nodiscard]] auto FindIt(const TKey& key) const -> const_iterator {
            return const_iterator(mMap.FindIt(key));
        }

        auto Insert(const TKey& key) -> bool {
            auto result = mMap.Emplace(key, FHashSetPlaceholderValue{});
            return result.second;
        }
        auto Insert(TKey&& key) -> bool {
            auto result = mMap.Emplace(Move(key), FHashSetPlaceholderValue{});
            return result.second;
        }

        template <typename... TArgs> auto Emplace(TArgs&&... args) -> bool {
            TKey key(Forward<TArgs>(args)...);
            return Insert(Move(key));
        }

        auto Erase(const TKey& key) -> size_type { return mMap.Erase(key); }
        auto Erase(iterator it) -> iterator { return iterator(mMap.Erase(it.MapIterator())); }
        auto Remove(const TKey& key) -> bool { return mMap.Remove(key); }

        void Clear() { mMap.Clear(); }

        void Reserve(size_type count) { mMap.Reserve(count); }

        void Rehash(size_type count) { mMap.Rehash(count); }

    private:
        FMapType mMap{};
    };

} // namespace AltinaEngine::Core::Container

#endif // ALTINAENGINE_CORE_PUBLIC_CONTAINER_ROBINHOODHASHSET_H
