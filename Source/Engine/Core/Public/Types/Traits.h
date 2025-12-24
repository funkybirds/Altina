#pragma once

#include "Aliases.h"

namespace AltinaEngine
{
    
    

    template <bool TValue> struct TBoolConstant
    {
        static constexpr bool Value = TValue;
    };

    using TTrueType  = TBoolConstant<true>;
    using TFalseType = TBoolConstant<false>;

    template <typename T> auto Declval() noexcept -> T&&;

    namespace Detail
    {

        template <typename T, typename... Args>
        auto TestConstructParen(int) -> decltype(T(Declval<Args>()...), TTrueType{});

        template <typename T, typename... Args> TFalseType TestConstructParen(...);

        template <typename T, typename... Args>
        auto TestConstructBrace(int) -> decltype(T{ Declval<Args>()... }, TTrueType{});

        template <typename T, typename... Args> TFalseType TestConstructBrace(...);

        template <typename T> auto       TestDestruct(int) -> decltype(Declval<T&>().~T(), TTrueType{});

        template <typename T> TFalseType TestDestruct(...);

        template <typename To, typename From>
        auto TestStaticCast(int) -> decltype(static_cast<To>(Declval<From>()), TTrueType{});

        template <typename To, typename From> TFalseType TestStaticCast(...);

        template <typename To, typename From>
        auto TestDynamicCast(int) -> decltype(dynamic_cast<To>(Declval<From>()), TTrueType{});

        template <typename To, typename From> TFalseType TestDynamicCast(...);

        template <typename T, typename U> auto TestLess(int) -> decltype(Declval<T>() < Declval<U>(), TTrueType{});

        template <typename T, typename U> TFalseType TestLess(...);

        template <typename T, typename U> auto TestEqual(int) -> decltype(Declval<T>() == Declval<U>(), TTrueType{});

        template <typename T, typename U> TFalseType TestEqual(...);

        template <typename T, typename U> auto TestGreater(int) -> decltype(Declval<T>() > Declval<U>(), TTrueType{});

        template <typename T, typename U> TFalseType TestGreater(...);

        template <typename T>
        auto TestIndexReadable(int) -> decltype(Declval<const T&>()[usize{}], TTrueType{});

        template <typename T> TFalseType TestIndexReadable(...);

        template <typename T>
        auto TestIndexWritable(int) -> decltype(Declval<T&>()[usize{}], TTrueType{});

        template <typename T> TFalseType TestIndexWritable(...);

        template <typename It>
        auto TestReadableIterator(int)
            -> decltype(*Declval<It&>(), ++Declval<It&>(), TTrueType{});

        template <typename It> TFalseType TestReadableIterator(...);

        template <typename It>
        auto TestWritableIterator(int)
            -> decltype(*Declval<It&>() = *Declval<It&>(), ++Declval<It&>(), TTrueType{});

        template <typename It> TFalseType TestWritableIterator(...);

        template <typename It>
        auto TestRandomAccessIterator(int)
            -> decltype(
                Declval<It&>() + usize{},
                Declval<It&>() - usize{},
                Declval<It&>() += usize{},
                Declval<It&>() -= usize{},
                Declval<It&>()[usize{}],
                Declval<It>() - Declval<It>(),
                TTrueType{}
            );

        template <typename It> TFalseType TestRandomAccessIterator(...);

    } // namespace Detail

    template <typename... Types> struct TTypeSet
    {
    };

    template <typename T, typename U> struct TTypeSameAs : TFalseType
    {
    };

    template <typename T> struct TTypeSameAs<T, T> : TTrueType
    {
    };

    template <typename T, typename TypeSet> struct TTypeIsAnyOf;

    template <typename T, typename... Candidates>
    struct TTypeIsAnyOf<T, TTypeSet<Candidates...>> : TBoolConstant<(... || TTypeSameAs<T, Candidates>::Value)>
    {
    };

    template <typename T> struct TTypeIsConstQualified : TFalseType
    {
    };

    template <typename T> struct TTypeIsConstQualified<const T> : TTrueType
    {
    };

    template <typename T> struct TTypeIsVolatileQualified : TFalseType
    {
    };

    template <typename T> struct TTypeIsVolatileQualified<volatile T> : TTrueType
    {
    };

    template <typename T> struct TTypeIsIntegral : TFalseType
    {
    };

    template <> struct TTypeIsIntegral<bool> : TTrueType
    {
    };
    template <> struct TTypeIsIntegral<char> : TTrueType
    {
    };
#if defined(__cpp_char8_t)
    template <> struct TTypeIsIntegral<char8_t> : TTrueType
    {
    };
#endif
    template <> struct TTypeIsIntegral<char16_t> : TTrueType
    {
    };
    template <> struct TTypeIsIntegral<char32_t> : TTrueType
    {
    };
    template <> struct TTypeIsIntegral<wchar_t> : TTrueType
    {
    };
    template <> struct TTypeIsIntegral<signed char> : TTrueType
    {
    };
    template <> struct TTypeIsIntegral<unsigned char> : TTrueType
    {
    };
    template <> struct TTypeIsIntegral<short> : TTrueType
    {
    };
    template <> struct TTypeIsIntegral<unsigned short> : TTrueType
    {
    };
    template <> struct TTypeIsIntegral<int> : TTrueType
    {
    };
    template <> struct TTypeIsIntegral<unsigned int> : TTrueType
    {
    };
    template <> struct TTypeIsIntegral<long> : TTrueType
    {
    };
    template <> struct TTypeIsIntegral<unsigned long> : TTrueType
    {
    };
    template <> struct TTypeIsIntegral<long long> : TTrueType
    {
    };
    template <> struct TTypeIsIntegral<unsigned long long> : TTrueType
    {
    };

    template <typename T, bool IsIntegral = TTypeIsIntegral<T>::Value> struct TTypeIsSigned : TFalseType
    {
    };

    template <typename T> struct TTypeIsSigned<T, true> : TBoolConstant<(static_cast<T>(-1) < static_cast<T>(0))>
    {
    };

    template <typename T> struct TTypeIsFloatingPoint : TFalseType
    {
    };

    template <> struct TTypeIsFloatingPoint<float> : TTrueType
    {
    };
    template <> struct TTypeIsFloatingPoint<double> : TTrueType
    {
    };
    template <> struct TTypeIsFloatingPoint<long double> : TTrueType
    {
    };

    template <typename T, typename... Args>
    struct TTypeIsConstructible :
        TBoolConstant<decltype(Detail::TestConstructParen<T, Args...>(0))::Value
            || decltype(Detail::TestConstructBrace<T, Args...>(0))::Value>
    {
    };

    template <typename T> struct TTypeIsDefaultConstructible : TTypeIsConstructible<T>
    {
    };

    template <typename T> struct TTypeIsCopyConstructible : TTypeIsConstructible<T, const T&>
    {
    };

    template <typename T> struct TTypeIsMovable : TTypeIsConstructible<T, T&&>
    {
    };

    template <typename T> struct TRemoveReference
    {
        using Type = T;
    };

    template <typename T> struct TRemoveReference<T&>
    {
        using Type = T;
    };

    template <typename T> struct TRemoveReference<T&&>
    {
        using Type = T;
    };

    template <typename T>
    constexpr typename TRemoveReference<T>::Type&& Move(T&& Arg) noexcept
    {
        return static_cast<typename TRemoveReference<T>::Type&&>(Arg);
    }

    template <typename T>
    constexpr T&& Forward(typename TRemoveReference<T>::Type& Arg) noexcept
    {
        return static_cast<T&&>(Arg);
    }

    template <typename T>
    constexpr T&& Forward(typename TRemoveReference<T>::Type&& Arg) noexcept
    {
        return static_cast<T&&>(Arg);
    }

    template <typename T> struct TRemoveConst
    {
        using Type = T;
    };

    template <typename T> struct TRemoveConst<const T>
    {
        using Type = T;
    };

    template <typename T> struct TRemoveVolatile
    {
        using Type = T;
    };

    template <typename T> struct TRemoveVolatile<volatile T>
    {
        using Type = T;
    };

    template <typename T> struct TRemoveCV
    {
        using Type = typename TRemoveConst<typename TRemoveVolatile<T>::Type>::Type;
    };

    template <typename T> struct TDecay
    {
        using Type = typename TRemoveCV<typename TRemoveReference<T>::Type>::Type;
    };

    template <typename T> struct TTypeIsDecayed : TTypeSameAs<T, typename TDecay<T>::Type>
    {
    };

    template <typename T> struct TTypeIsDestructible : decltype(Detail::TestDestruct<T>(0))
    {
    };

    template <typename T, typename U> struct TTypeIsStaticConvertible : decltype(Detail::TestStaticCast<T, U>(0))
    {
    };

    template <typename T, typename U> struct TTypeIsDynamicConvertible : decltype(Detail::TestDynamicCast<T, U>(0))
    {
    };

    template <typename T, typename U = T> struct TTypeLessComparable : decltype(Detail::TestLess<T, U>(0))
    {
    };

    template <typename T, typename U = T> struct TTypeEqualComparable : decltype(Detail::TestEqual<T, U>(0))
    {
    };

    template <typename T, typename U = T> struct TTypeGreaterComparable : decltype(Detail::TestGreater<T, U>(0))
    {
    };

    template <typename T> struct TTypeIsRandomReadable : decltype(Detail::TestIndexReadable<T>(0))
    {
    };

    template <typename T> struct TTypeIsRandomWritable : decltype(Detail::TestIndexWritable<T>(0))
    {
    };

    template <typename It> struct TTypeIsReadableIterator : decltype(Detail::TestReadableIterator<It>(0))
    {
    };

    template <typename It> struct TTypeIsWritableIterator : decltype(Detail::TestWritableIterator<It>(0))
    {
    };

    template <typename It> struct TTypeIsRandomAccessIterator : decltype(Detail::TestRandomAccessIterator<It>(0))
    {
    };

    // Backwards-compatible alias the user requested
    template <typename T, typename U = T> using TGreaterComparable = TTypeGreaterComparable<T, U>;

    template <typename T, typename U> inline constexpr bool TTypeSameAs_v = TTypeSameAs<T, U>::Value;

    template <typename T> inline constexpr bool             TTypeIsIntegral_v = TTypeIsIntegral<T>::Value;

    template <typename T> inline constexpr bool             TTypeIsFloatingPoint_v = TTypeIsFloatingPoint<T>::Value;

    template <typename T> inline constexpr bool             TTypeIsConstQualified_v = TTypeIsConstQualified<T>::Value;

    template <typename T> inline constexpr bool TTypeIsVolatileQualified_v = TTypeIsVolatileQualified<T>::Value;

    template <typename T> inline constexpr bool TTypeIsDefaultConstructible_v = TTypeIsDefaultConstructible<T>::Value;

    template <typename T> inline constexpr bool TTypeIsCopyConstructible_v = TTypeIsCopyConstructible<T>::Value;

    template <typename T> inline constexpr bool TTypeIsMovable_v = TTypeIsMovable<T>::Value;

    template <typename T> inline constexpr bool TTypeIsDestructible_v = TTypeIsDestructible<T>::Value;

    template <typename T, typename U>
    inline constexpr bool TTypeIsStaticConvertible_v = TTypeIsStaticConvertible<T, U>::Value;

    template <typename T, typename U>
    inline constexpr bool TTypeIsDynamicConvertible_v = TTypeIsDynamicConvertible<T, U>::Value;

    template <typename T, typename U = T>
    inline constexpr bool TTypeLessComparable_v = TTypeLessComparable<T, U>::Value;

    template <typename T, typename U = T>
    inline constexpr bool TTypeEqualComparable_v = TTypeEqualComparable<T, U>::Value;

    template <typename T, typename U = T>
    inline constexpr bool TTypeGreaterComparable_v = TTypeGreaterComparable<T, U>::Value;

    // Comparator instances will be declared after the comparator templates to avoid
    // referencing templates before their definitions.

    template <typename T = void> struct TLess
    {
        constexpr bool operator()(const T& a, const T& b) const noexcept(noexcept(a < b)) { return a < b; }
    };

    template <> struct TLess<void>
    {
        template <typename L, typename R>
        constexpr auto operator()(L&& l, R&& r) const -> decltype(Declval<L>() < Declval<R>())
        {
            return static_cast<L&&>(l) < static_cast<R&&>(r);
        }
    };

    template <typename T = void> struct TGreater
    {
        constexpr bool operator()(const T& a, const T& b) const noexcept(noexcept(a > b)) { return a > b; }
    };

    template <> struct TGreater<void>
    {
        template <typename L, typename R>
        constexpr auto operator()(L&& l, R&& r) const -> decltype(Declval<L>() > Declval<R>())
        {
            return static_cast<L&&>(l) > static_cast<R&&>(r);
        }
    };

    template <typename T = void> struct TEqual
    {
        constexpr bool operator()(const T& a, const T& b) const noexcept(noexcept(a == b)) { return a == b; }
    };

    template <> struct TEqual<void>
    {
        template <typename L, typename R>
        constexpr auto operator()(L&& l, R&& r) const -> decltype(Declval<L>() == Declval<R>())
        {
            return static_cast<L&&>(l) == static_cast<R&&>(r);
        }
    };

    // Const evaluated context
    constexpr bool IsConstantEvaluated() noexcept
    {
        if consteval
        {
            return true;
        }
        else
        {
            return false;
        }
    }

} // namespace AltinaEngine
