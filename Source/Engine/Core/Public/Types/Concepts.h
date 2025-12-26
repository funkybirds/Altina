#pragma once

#include "Aliases.h"
#include "Traits.h"

namespace AltinaEngine
{
    template <typename T>
    concept IScalar = TTypeIsIntegral<T>::Value || TTypeIsFloatingPoint<T>::Value;

    template <typename R, typename Pred>
    concept IPredicateForRange = requires(Pred p, decltype(Declval<R>().begin()) it)
    {
        { p(*it) };
        { static_cast<bool>(p(*it)) };
    };
    template <typename T>
    concept IIntegral = TTypeIsIntegral<T>::Value;

    template <typename T>
    concept IFloatingPoint = TTypeIsFloatingPoint<T>::Value;

    template <typename T>
    concept ISignedIntegral = TTypeIsIntegral<T>::Value && TTypeIsSigned<T>::Value;

    template <typename T, typename U>
    concept ISameSizeAs = sizeof(T) == sizeof(U);

    template <typename T, typename... Ts>
    concept ISameAsAll = (sizeof...(Ts) == 0) || (TTypeSameAs<T, Ts>::Value && ...);

    template <typename T>
    concept IRandomReadable = TTypeIsRandomReadable<T>::Value;

    template <typename T>
    concept IRandomWritable = TTypeIsRandomWritable<T>::Value;

    template <typename It>
    concept IReadableIterator = TTypeIsReadableIterator<It>::Value;

    template <typename It>
    concept IWritableIterator = TTypeIsWritableIterator<It>::Value;

    template <typename It>
    concept IRandomAccessIterator = TTypeIsRandomAccessIterator<It>::Value;

    template <typename From, typename To>
    concept IDynamicConvertible = TTypeIsDynamicConvertible<To, From>::Value;

    // Range concepts
    template <typename R>
    concept IRange = requires(R r)
    {
        { r.begin() };
        { r.end() };
    };

    template <typename R>
    concept ICommonRange = IRange<R> && TTypeSameAs<decltype(Declval<R>().begin()), decltype(Declval<R>().end())>::Value;

    template <typename R>
    concept IReadableRange = IRange<R> && IReadableIterator<decltype(Declval<R>().begin())>;

    template <typename R>
    concept IWritableRange = IRange<R> && IWritableIterator<decltype(Declval<R>().begin())>;

    template <typename It>
    concept IIncrementable = requires(It it)
    {
        { ++it };
        { it++ };
    };

    template <typename R>
    concept IForwardRange = IRange<R> && IIncrementable<decltype(Declval<R>().begin())> && IReadableIterator<decltype(Declval<R>().begin())>;

        template <typename T>
        concept ICharType = TTypeSameAs_v<T, char>
    #if defined(__cpp_char8_t)
                            || TTypeSameAs_v<T, char8_t>
    #endif
                            || TTypeSameAs_v<T, char16_t>
                            || TTypeSameAs_v<T, char32_t>
                            || TTypeSameAs_v<T, wchar_t>;
} // namespace AltinaEngine
