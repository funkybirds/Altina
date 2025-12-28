#ifndef ALTINAENGINE_CORE_PUBLIC_CONTAINER_HASHSET_H
#define ALTINAENGINE_CORE_PUBLIC_CONTAINER_HASHSET_H

#include <unordered_set>

#include "../Base/CoreAPI.h"
#include "../Types/Aliases.h"

namespace AltinaEngine::Core::Container
{
    // Templated wrapper class for std::unordered_set. Provides a stable
    // `THashSet` type while allowing the underlying implementation to be
    // switched later if needed.
    template <typename Key, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>,
        typename Allocator = std::allocator<Key>>
    class THashSet : public std::unordered_set<Key, Hash, KeyEqual, Allocator>
    {
    public:
        using TBase = std::unordered_set<Key, Hash, KeyEqual, Allocator>;
        using typename TBase::allocator_type;
        using typename TBase::key_type;
        using typename TBase::size_type;
        using typename TBase::value_type;

        THashSet() noexcept(std::is_nothrow_default_constructible_v<TBase>) : TBase() {}

        explicit THashSet(size_type bucket_count, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual(),
            const Allocator& alloc = Allocator())
            : TBase(bucket_count, hash, equal, alloc)
        {
        }

        template <class InputIt>
        THashSet(InputIt first, InputIt last, size_type bucket_count = 0, const Hash& hash = Hash(),
            const KeyEqual& equal = KeyEqual(), const Allocator& alloc = Allocator())
            : TBase(first, last, bucket_count, hash, equal, alloc)
        {
        }

        THashSet(std::initializer_list<value_type> init, size_type bucket_count = 0, const Hash& hash = Hash(),
            const KeyEqual& equal = KeyEqual(), const Allocator& alloc = Allocator())
            : TBase(init, bucket_count, hash, equal, alloc)
        {
        }

        THashSet(const THashSet& other) : TBase(other) {}
        THashSet(THashSet&& other) noexcept(std::is_nothrow_move_constructible_v<TBase>) : TBase(std::move(other)) {}
        auto operator=(const THashSet& other) -> THashSet&
        {
            TBase::operator=(other);
            return *this;
        }
        auto operator=(THashSet&& other) noexcept(std::is_nothrow_move_assignable_v<TBase>) -> THashSet&
        {
            TBase::operator=(std::move(other));
            return *this;
        }
        ~THashSet() = default;
    };

} // namespace AltinaEngine::Core::Container

#endif // ALTINAENGINE_CORE_PUBLIC_CONTAINER_HASHSET_H
