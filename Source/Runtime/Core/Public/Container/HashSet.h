#ifndef ALTINAENGINE_CORE_PUBLIC_CONTAINER_HASHSET_H
#define ALTINAENGINE_CORE_PUBLIC_CONTAINER_HASHSET_H

#include "HashUtility.h"
#include "RobinHoodHashSet.h"

namespace AltinaEngine::Core::Container {
    template <typename Key, typename Hash = THashFunc<Key>, typename KeyEqual = TEqual<Key>,
        typename Allocator = void>
    using THashSet = TRobinHoodHashSet<Key, Hash, KeyEqual>;
} // namespace AltinaEngine::Core::Container

#endif // ALTINAENGINE_CORE_PUBLIC_CONTAINER_HASHSET_H
