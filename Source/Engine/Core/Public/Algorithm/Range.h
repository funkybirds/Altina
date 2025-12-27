#pragma once

#include "Types/Concepts.h"
#include "Types/Traits.h"

namespace AltinaEngine::Core::Algorithm
{
    template <IForwardRange R, typename Comp = TLess<>>
    [[nodiscard]] constexpr auto MaxElement(R&& range, Comp comp = Comp{})
    {
        auto it = range.begin();
        if (it == range.end())
            return it;

        auto best = it;
        ++it;
        for (; it != range.end(); ++it)
        {
            if (comp(*best, *it))
                best = it;
        }
        return best;
    }

    template <IForwardRange R, typename Comp = AltinaEngine::TLess<>>
    [[nodiscard]] constexpr auto MinElement(R&& range, Comp comp = Comp{})
    {
        auto it = range.begin();
        if (it == range.end())
            return it;

        auto best = it;
        ++it;
        for (; it != range.end(); ++it)
        {
            if (comp(*it, *best))
                best = it;
        }
        return best;
    }

    template <IRange R, typename Pred> [[nodiscard]] constexpr auto AnyOf(R&& range, Pred pred) -> bool
    {
        for (auto it = range.begin(); it != range.end(); ++it)
            if (pred(*it))
                return true;
        return false;
    }

    template <IRange R, typename Pred>
        requires IPredicateForRange<R, Pred>
    [[nodiscard]] constexpr auto AllOf(R&& range, Pred pred) -> bool
    {
        for (auto it = range.begin(); it != range.end(); ++it)
            if (!pred(*it))
                return false;
        return true;
    }

    template <IRange R, typename Pred>
        requires IPredicateForRange<R, Pred>
    [[nodiscard]] constexpr auto NoneOf(R&& range, Pred pred) -> bool
    {
        return !AnyOf(static_cast<R&&>(range), pred);
    }

    template <IRange R, typename Comp = TLess<>>
    [[nodiscard]] constexpr auto IsSorted(R&& range, Comp comp = Comp{}) -> bool
    {
        if (range.begin() == range.end())
            return true;
        auto prev = range.begin();
        auto it   = prev;
        ++it;
        for (; it != range.end(); ++it, ++prev)
        {
            if (comp(*it, *prev))
                return false;
        }
        return true;
    }

} // namespace AltinaEngine::Core::Algorithm
