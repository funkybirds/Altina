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

    template <IRange R, typename Pred>
    [[nodiscard]] constexpr bool AnyOf(R&& range, Pred pred)
    {
        for (auto it = range.begin(); it != range.end(); ++it)
            if (pred(*it))
                return true;
        return false;
    }

    template <IRange R, typename Pred>
    requires IPredicateForRange<R, Pred>
    [[nodiscard]] constexpr bool AllOf(R&& range, Pred pred)
    {
        for (auto it = range.begin(); it != range.end(); ++it)
            if (!pred(*it))
                return false;
        return true;
    }

    template <IRange R, typename Pred>
    requires IPredicateForRange<R, Pred>
    [[nodiscard]] constexpr bool NoneOf(R&& range, Pred pred)
    {
        return !AnyOf(static_cast<R&&>(range), pred);
    }

    template <IRange R, typename Comp = TLess<>>
    [[nodiscard]] constexpr bool IsSorted(R&& range, Comp comp = Comp{})
    {
        if (range.begin() == range.end())
            return true;
        auto prev = range.begin();
        auto it = prev;
        ++it;
        for (; it != range.end(); ++it, ++prev)
        {
            if (comp(*it, *prev))
                return false;
        }
        return true;
    }

} // namespace Altina::Core::Algorithm
