#ifndef ALTINAENGINE_CORE_PUBLIC_CONTAINER_HASHMAP_H
#define ALTINAENGINE_CORE_PUBLIC_CONTAINER_HASHMAP_H

#include "HashUtility.h"
#include "RobinHoodHashMap.h"

namespace AltinaEngine::Core::Container {
    template <typename Key, typename T, typename Hash = THashFunc<Key>,
        typename KeyEqual = TEqual<Key>, typename Allocator = void>
    using THashMap = TRobinHoodHashMap<Key, T, Hash, KeyEqual>;
} // namespace AltinaEngine::Core::Container

#endif // ALTINAENGINE_CORE_PUBLIC_CONTAINER_HASHMAP_H
