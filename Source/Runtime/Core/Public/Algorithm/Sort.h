#pragma once

#include "Types/Concepts.h"
#include "Types/Traits.h"

using AltinaEngine::Move;
using AltinaEngine::TLess;

namespace AltinaEngine::Core::Algorithm {
    namespace Detail::Sort {
        template <typename T>
        constexpr void Swap(T& a, T& b) noexcept(
            noexcept(T{ Move(a) }) && noexcept(a = Move(b)) && noexcept(b = Move(a))) {
            if (&a == &b) {
                return;
            }
            T tmp = Move(a);
            a     = Move(b);
            b     = Move(tmp);
        }

        template <typename It>
        constexpr void IterSwap(It a, It b) noexcept(noexcept(Swap(*a, *b))) {
            Swap(*a, *b);
        }

        constexpr auto Log2Floor(usize value) noexcept -> isize {
            isize result = 0;
            while (value > 1) {
                value >>= 1;
                ++result;
            }
            return result;
        }

        template <typename It, typename Comp>
        constexpr auto MedianOfThree(It a, It b, It c, Comp& comp) -> It {
            // Return iterator to median element under strict-weak-order comparator.
            if (comp(*a, *b)) {
                if (comp(*b, *c))
                    return b;
                return comp(*a, *c) ? c : a;
            }
            if (comp(*a, *c))
                return a;
            return comp(*b, *c) ? c : b;
        }

        template <typename It, typename Comp>
        constexpr auto Partition(It first, It last, Comp& comp) -> It {
            const auto count = static_cast<isize>(last - first);
            const auto mid   = first + (count / 2);

            It         pivotIt = MedianOfThree(first, mid, last - 1, comp);
            IterSwap(pivotIt, last - 1);

            It store = first;
            for (It it = first; it != (last - 1); ++it) {
                if (comp(*it, *(last - 1))) {
                    IterSwap(store, it);
                    ++store;
                }
            }
            IterSwap(store, last - 1);
            return store;
        }

        template <typename It, typename Comp>
        constexpr void InsertionSort(It first, It last, Comp& comp) {
            if (last - first <= 1)
                return;

            for (It it = first + 1; it != last; ++it) {
                auto value = Move(*it);

                It   hole = it;
                while (hole != first && comp(value, *(hole - 1))) {
                    *hole = Move(*(hole - 1));
                    --hole;
                }
                *hole = Move(value);
            }
        }

        template <typename It, typename Comp>
        constexpr void SiftDown(It first, isize startIndex, isize count, Comp& comp) {
            isize root = startIndex;
            while (true) {
                const isize left = root * 2 + 1;
                if (left >= count)
                    return;

                isize best = root;
                if (comp(first[best], first[left])) {
                    best = left;
                }

                const isize right = left + 1;
                if (right < count && comp(first[best], first[right])) {
                    best = right;
                }

                if (best == root)
                    return;

                IterSwap(first + root, first + best);
                root = best;
            }
        }

        template <typename It, typename Comp>
        constexpr void HeapSort(It first, It last, Comp& comp) {
            const isize count = static_cast<isize>(last - first);
            if (count <= 1)
                return;

            // Heapify
            for (isize start = (count - 2) / 2; start >= 0; --start) {
                SiftDown(first, start, count, comp);
                if (start == 0) // avoid underflow on signed loop.
                    break;
            }

            // Pop max to the end.
            for (isize end = count - 1; end > 0; --end) {
                IterSwap(first, first + end);
                SiftDown(first, 0, end, comp);
            }
        }

        template <typename It, typename Comp>
        constexpr void IntroSort(It first, It last, Comp& comp, isize depthLimit) {
            constexpr isize kInsertionThreshold = 16;

            while ((last - first) > kInsertionThreshold) {
                if (depthLimit <= 0) {
                    HeapSort(first, last, comp);
                    return;
                }

                --depthLimit;
                It          pivot = Partition(first, last, comp);

                // Recurse into the smaller partition to bound recursion depth.
                const isize leftCount  = static_cast<isize>(pivot - first);
                const isize rightCount = static_cast<isize>(last - (pivot + 1));
                if (leftCount < rightCount) {
                    IntroSort(first, pivot, comp, depthLimit);
                    first = pivot + 1;
                } else {
                    IntroSort(pivot + 1, last, comp, depthLimit);
                    last = pivot;
                }
            }

            InsertionSort(first, last, comp);
        }
    } // namespace Detail::Sort

    template <typename It, typename Comp = TLess<>>
        requires AltinaEngine::CRandomAccessIterator<It> && AltinaEngine::CWritableIterator<It>
        && requires(Comp c, decltype(*Declval<It>()) a, decltype(*Declval<It>()) b) {
               { c(a, b) } -> AltinaEngine::CStaticConvertible<bool>;
           }
    constexpr void Sort(It first, It last, Comp comp = Comp{}) {
        const auto count = static_cast<isize>(last - first);
        if (count <= 1)
            return;

        isize depthLimit = 2 * Detail::Sort::Log2Floor(static_cast<usize>(count));
        Detail::Sort::IntroSort(first, last, comp, depthLimit);
    }

    template <AltinaEngine::CRange R, typename Comp = TLess<>>
        requires AltinaEngine::CRandomAccessIterator<decltype(Declval<R>().begin())>
        && AltinaEngine::CWritableIterator<decltype(Declval<R>().begin())>
        && requires(Comp c, decltype(*Declval<decltype(Declval<R>().begin())>()) a,
            decltype(*Declval<decltype(Declval<R>().begin())>()) b) {
               { c(a, b) } -> AltinaEngine::CStaticConvertible<bool>;
           }
    constexpr void Sort(R&& range, Comp comp = Comp{}) {
        Sort(range.begin(), range.end(), Move(comp));
    }

} // namespace AltinaEngine::Core::Algorithm
