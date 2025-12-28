#pragma once
#include "../Types/Traits.h"
#include "../Types/Aliases.h"
#include "IndexSequence.h"

namespace AltinaEngine::Core::Container
{
    using AltinaEngine::usize;

    // Helper to get Nth type from parameter pack
    template <usize I, typename... Ts> struct TNthType;
    template <typename T, typename... Ts> struct TNthType<0, T, Ts...>
    {
        using type = T;
    };
    template <usize I, typename T, typename... Ts> struct TNthType<I, T, Ts...>
    {
        using type = TNthType<I - 1, Ts...>::type;
    };

    template <usize I, typename T> struct TTupleElement
    {
        using TValue = T;
        TValue mValue{};

        constexpr TTupleElement() = default;
        template <typename U> constexpr explicit TTupleElement(U&& v) : mValue(Forward<U>(v)) {}
    };

    template <typename... Ts> struct TTuple;

    namespace Detail
    {
        template <typename Indices, typename... Ts> struct TTupleImpl;

        template <usize... Is, typename... Ts>
        struct TTupleImpl<TIndexSequence<Is...>, Ts...> : TTupleElement<Is, Ts>...
        {
            using Self             = TTupleImpl;
            constexpr TTupleImpl() = default;

            template <typename... Us>
            constexpr explicit TTupleImpl(Us&&... us) : TTupleElement<Is, Ts>(Forward<Us>(us))...
            {
            }
        };
    } // namespace Detail

    template <typename... Ts> struct TTuple : Detail::TTupleImpl<TMakeIndexSequence<sizeof...(Ts)>, Ts...>
    {
        using Base = Detail::TTupleImpl<TMakeIndexSequence<sizeof...(Ts)>, Ts...>;
        static constexpr usize Size() noexcept { return sizeof...(Ts); }

        constexpr TTuple() = default;

        template <typename... Us> constexpr explicit TTuple(Us&&... us) : Base(Forward<Us>(us)...) {}
    };

    // Get helpers
    template <usize I, typename... Ts> constexpr auto& Get(TTuple<Ts...>& t)
    {
        using TElement = TTupleElement<I, typename TNthType<I, Ts...>::type>;
        return static_cast<TElement&>(t).mValue;
    }

    template <usize I, typename... Ts> constexpr auto const& Get(TTuple<Ts...> const& t)
    {
        using TElement = TTupleElement<I, typename TNthType<I, Ts...>::type>;
        return static_cast<TElement const&>(t).mValue;
    }

    template <usize I, typename... Ts> constexpr auto&& Get(TTuple<Ts...>&& t)
    {
        using TElement = TTupleElement<I, typename TNthType<I, Ts...>::type>;
        return static_cast<TElement&&>(t).mValue;
    }

} // namespace AltinaEngine::Core::Container
