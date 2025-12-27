#ifndef ALTINAENGINE_CORE_PUBLIC_CONTAINER_HASHMAP_H
#define ALTINAENGINE_CORE_PUBLIC_CONTAINER_HASHMAP_H

#include <unordered_map>

#include "../Base/CoreAPI.h"
#include "../Types/Aliases.h"

namespace AltinaEngine::Core::Container
{
    // Templated wrapper class for std::unordered_map. Implemented as a thin
    // subclass so call-sites can treat `THashMap` like the standard container
    // while allowing us to replace the implementation later if needed.
    template <typename Key, typename T, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>,
        typename Allocator = std::allocator<std::pair<const Key, T>>>
    class THashMap : public std::unordered_map<Key, T, Hash, KeyEqual, Allocator>
    {
    public:
        using Base = std::unordered_map<Key, T, Hash, KeyEqual, Allocator>;
        using typename Base::allocator_type;
        using typename Base::key_type;
        using typename Base::mapped_type;
        using typename Base::size_type;
        using typename Base::value_type;

        THashMap() noexcept(std::is_nothrow_default_constructible<Base>::value) : Base() {}

        explicit THashMap(size_type bucket_count, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual(),
            const Allocator& alloc = Allocator())
            : Base(bucket_count, hash, equal, alloc)
        {
        }

        template <class InputIt>
        THashMap(InputIt first, InputIt last, size_type bucket_count = 0, const Hash& hash = Hash(),
            const KeyEqual& equal = KeyEqual(), const Allocator& alloc = Allocator())
            : Base(first, last, bucket_count, hash, equal, alloc)
        {
        }

        THashMap(std::initializer_list<value_type> init, size_type bucket_count = 0, const Hash& hash = Hash(),
            const KeyEqual& equal = KeyEqual(), const Allocator& alloc = Allocator())
            : Base(init, bucket_count, hash, equal, alloc)
        {
        }

        THashMap(const THashMap& other) : Base(other) {}
        THashMap(THashMap&& other) noexcept(std::is_nothrow_move_constructible<Base>::value) : Base(std::move(other)) {}
        THashMap& operator=(const THashMap& other)
        {
            Base::operator=(other);
            return *this;
        }
        THashMap& operator=(THashMap&& other) noexcept(std::is_nothrow_move_assignable<Base>::value)
        {
            Base::operator=(std::move(other));
            return *this;
        }
        ~THashMap() = default;
    };

} // namespace AltinaEngine::Core::Container

#endif // ALTINAENGINE_CORE_PUBLIC_CONTAINER_HASHMAP_H
