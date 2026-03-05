#ifndef ALTINAENGINE_CORE_PUBLIC_CONTAINER_PAIR_H
#define ALTINAENGINE_CORE_PUBLIC_CONTAINER_PAIR_H

#include "../Types/Traits.h"

namespace AltinaEngine::Core::Container {
    template <typename TFirst, typename TSecond> struct TPair {
        TFirst  first{};
        TSecond second{};

        constexpr TPair() = default;

        constexpr TPair(const TFirst& inFirst, const TSecond& inSecond)
            : first(inFirst), second(inSecond) {}

        constexpr TPair(TFirst&& inFirst, TSecond&& inSecond)
            : first(Move(inFirst)), second(Move(inSecond)) {}

        template <typename UFirst, typename USecond>
        constexpr TPair(UFirst&& inFirst, USecond&& inSecond)
            : first(Forward<UFirst>(inFirst)), second(Forward<USecond>(inSecond)) {}
    };
} // namespace AltinaEngine::Core::Container

#endif // ALTINAENGINE_CORE_PUBLIC_CONTAINER_PAIR_H
