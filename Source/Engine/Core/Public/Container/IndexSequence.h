#pragma once

#include "../Types/Aliases.h"

namespace AltinaEngine::Core::Container
{
    using AltinaEngine::usize;

    template <usize... Is> struct TIndexSequence
    {
        using type = TIndexSequence;
        static constexpr usize Size() noexcept { return sizeof...(Is); }
    };

    template <usize N, usize... Is> struct TMakeIndexSequenceImpl : TMakeIndexSequenceImpl<N - 1, N - 1, Is...>
    {
    };

    template <usize... Is> struct TMakeIndexSequenceImpl<0, Is...>
    {
        using type = TIndexSequence<Is...>;
    };

    template <usize N> using TMakeIndexSequence = TMakeIndexSequenceImpl<N>::type;

} // namespace AltinaEngine::Core::Container
