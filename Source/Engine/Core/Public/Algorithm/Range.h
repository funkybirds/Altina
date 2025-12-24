#pragma once

#include "Types/Concepts.h"
#include "Types/Traits.h"

namespace AltinaEngine
{
    

    template <IForwardRange R>
    constexpr auto MaxElement(R&& range)
    {
        auto it = range.begin();
        if (it == range.end())
            return it;

        auto best = it;
        ++it;
        for (; it != range.end(); ++it)
        {
            if (TLess<>()(*best, *it))
                best = it;
        }
        return best;
    }

    template <IForwardRange R>
    constexpr auto MinElement(R&& range)
    {
        auto it = range.begin();
        if (it == range.end())
            return it;

        auto best = it;
        ++it;
        for (; it != range.end(); ++it)
        {
            if (TLess<>()(*it, *best))
                best = it;
        }
        return best;
    }

    template <IRange R, typename Pred>
    constexpr bool AnyOf(R&& range, Pred pred)
    {
        for (auto it = range.begin(); it != range.end(); ++it)
            if (pred(*it))
                return true;
        return false;
    }

    template <IRange R, typename Pred>
    constexpr bool AllOf(R&& range, Pred pred)
    {
        for (auto it = range.begin(); it != range.end(); ++it)
            if (!pred(*it))
                return false;
        return true;
    }

    template <IRange R, typename Pred>
    constexpr bool NoneOf(R&& range, Pred pred)
    {
        return !AnyOf(static_cast<R&&>(range), pred);
    }

    template <IRange R>
    constexpr bool IsSorted(R&& range)
    {
        if (range.begin() == range.end())
            return true;
        auto prev = range.begin();
        auto it = prev;
        ++it;
        for (; it != range.end(); ++it, ++prev)
        {
            if (TLess<>()(*it, *prev))
                return false;
        }
        return true;
    }

} // namespace AltinaEngine
