#ifndef ALTINAENGINE_CORE_PUBLIC_CONTAINER_HASHMAP_H
#define ALTINAENGINE_CORE_PUBLIC_CONTAINER_HASHMAP_H

#include <unordered_map>

namespace AltinaEngine::Core::Container
{
    template <typename Key, typename T, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>,
        typename Allocator = std::allocator<std::pair<const Key, T>>>
    class THashMap : public std::unordered_map<Key, T, Hash, KeyEqual, Allocator>
    {
    public:
        using TBase = std::unordered_map<Key, T, Hash, KeyEqual, Allocator>;
        using typename TBase::allocator_type;
        using typename TBase::key_type;
        using typename TBase::mapped_type;
        using typename TBase::size_type;
        using typename TBase::value_type;

        THashMap() noexcept(std::is_nothrow_default_constructible_v<TBase>) : TBase() {}

        explicit THashMap(size_type bucket_count, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual(),
            const Allocator& alloc = Allocator())
            : TBase(bucket_count, hash, equal, alloc)
        {
        }

        template <class InputIt>
        THashMap(InputIt first, InputIt last, size_type bucket_count = 0, const Hash& hash = Hash(),
            const KeyEqual& equal = KeyEqual(), const Allocator& alloc = Allocator())
            : TBase(first, last, bucket_count, hash, equal, alloc)
        {
        }

        THashMap(std::initializer_list<value_type> init, size_type bucket_count = 0, const Hash& hash = Hash(),
            const KeyEqual& equal = KeyEqual(), const Allocator& alloc = Allocator())
            : TBase(init, bucket_count, hash, equal, alloc)
        {
        }

        THashMap(const THashMap& other) : TBase(other) {}
        THashMap(THashMap&& other) noexcept(std::is_nothrow_move_constructible_v<TBase>) : TBase(std::move(other)) {}
        auto operator=(const THashMap& other) -> THashMap&
        {
            TBase::operator=(other);
            return *this;
        }
        auto operator=(THashMap&& other) noexcept(std::is_nothrow_move_assignable_v<TBase>) -> THashMap&
        {
            TBase::operator=(std::move(other));
            return *this;
        }
        ~THashMap() = default;
    };

} // namespace AltinaEngine::Core::Container

#endif // ALTINAENGINE_CORE_PUBLIC_CONTAINER_HASHMAP_H
