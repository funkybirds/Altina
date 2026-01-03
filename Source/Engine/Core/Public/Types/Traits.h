#pragma once

#include "InternalTraits.h"
#include "Aliases.h"
#include "Container/Tuple.h"

// Forward-declare TTuple to avoid ordering issues when used in traits specializations

namespace AltinaEngine::Core::Container {
    template <typename...> struct TTuple;
}

namespace AltinaEngine {
    // Begin Traits
    template <typename...> using TVoid = void;
    template <typename...> struct TTypeSet;

    // Logical Predicates
    template <typename, typename> struct TTypeSameAs : TFalseType {};
    template <typename T> struct TTypeSameAs<T, T> : TTrueType {};

    template <typename T, typename TypeSet> struct TTypeIsAnyOf;

    template <typename T, typename... TCandidates>
    struct TTypeIsAnyOf<T, TTypeSet<TCandidates...>> :
        TBoolConstant<(... || TTypeSameAs<T, TCandidates>::Value)> {};

    template <typename T, typename U>
    concept CSameAs = TTypeSameAs<T, U>::Value;

    template <typename T, typename... Ts>
    concept CSameAsAll = (sizeof...(Ts) == 0) || (TTypeSameAs<T, Ts>::Value && ...);

    template <typename T>
    concept CDefinedType = requires { sizeof(T); };

    template <typename T, typename U>
    concept CSameSizeAs = sizeof(T) == sizeof(U);

    // Const-Volatile Qualifier Utilities
    template <typename> struct TTypeIsConstQualified : TFalseType {};
    template <typename T> struct TTypeIsConstQualified<const T> : TTrueType {};
    template <typename> struct TTypeIsVolatileQualified : TFalseType {};
    template <typename T> struct TTypeIsVolatileQualified<volatile T> : TTrueType {};
    template <typename> struct TTypeIsCVQualified : TFalseType {};
    template <typename T> struct TTypeIsConstQualified<const volatile T> : TTrueType {};

    template <typename T> struct TRemoveConst {
        using TType = T;
    };

    template <typename T> struct TRemoveConst<const T> {
        using TType = T;
    };

    template <typename T> struct TRemoveVolatile {
        using TType = T;
    };

    template <typename T> struct TRemoveVolatile<volatile T> {
        using TType = T;
    };

    template <typename T> struct TRemoveCV {
        using TType = TRemoveConst<typename TRemoveVolatile<T>::TType>::TType;
    };

    // Reference
    template <typename T> struct TRemoveReference {
        using TType = T;
    };

    template <typename T> struct TRemoveReference<T&> {
        using TType = T;
    };

    template <typename T> struct TRemoveReference<T&&> {
        using TType = T;
    };

    // Decay / Move / Forward

    // NOLINTBEGIN(cppcoreguidelines-missing-std-forward)
    // NOLINTBEGIN(cppcoreguidelines-rvalue-reference-param-not-moved)
    template <typename T> constexpr auto Move(T&& Arg) noexcept -> TRemoveReference<T>::TType&& {
        return static_cast<TRemoveReference<T>::TType&&>(Arg);
    }
    template <typename T>
    constexpr auto Forward(typename TRemoveReference<T>::TType& Arg) noexcept -> T&& {
        return static_cast<T&&>(Arg);
    }
    template <typename T>
    constexpr auto Forward(typename TRemoveReference<T>::TType&& Arg) noexcept -> T&& {
        return static_cast<T&&>(Arg);
    }
    template <typename T> struct TDecay {
        using TType = TRemoveCV<typename TRemoveReference<T>::TType>::TType;
    };
    // NOLINTEND(cppcoreguidelines-rvalue-reference-param-not-moved)
    // NOLINTEND(cppcoreguidelines-missing-std-forward)

    template <typename T> struct TTypeIsDecayed : TTypeSameAs<T, typename TDecay<T>::TType> {};
    template <typename T>
    concept CDecayed = TTypeIsDecayed<T>::Value;

    // Void
    template <typename T> struct TTypeIsVoid : TTypeSameAs<void, typename TRemoveCV<T>::TType> {};

    template <typename T>
    concept CNonVoid = !TTypeIsVoid<T>::Value;
    template <typename T>
    concept CVoid = TTypeIsVoid<T>::Value;

    // Enum & Union & Class
    template <typename T> struct TTypeIsUnion : Detail::CompilerTraits::TTypeIsUnionImpl<T> {};
    template <typename T> struct TTypeIsEnum : Detail::CompilerTraits::TTypeIsEnumImpl<T> {};

    namespace Detail::ClassPtr {
        template <class T> auto TestClassPtr(int T::*) -> TBoolConstant<!TTypeIsUnion<T>::Value>;
        template <class> auto   TestClassPtr(...) -> TBoolConstant<false>;
    } // namespace Detail::ClassPtr

    template <typename T>
    struct TTypeIsClass : decltype(Detail::ClassPtr::TestClassPtr<T>(nullptr)) {};

    template <typename T>
    concept CUnion = TTypeIsUnion<T>::Value;
    template <typename T>
    concept CEnum = TTypeIsEnum<T>::Value;
    template <typename T>
    concept CClass = TTypeIsClass<T>::Value;

    template <typename T>
        requires TTypeIsEnum<T>::Value
    using TUnderlyingType = Detail::CompilerTraits::TUnderlyingTypeImpl<T>;

    // Basic Types
    namespace Detail::BasicTypes {
        template <typename> struct TTypeIsIntegral : TFalseType {};
        template <> struct TTypeIsIntegral<bool> : TTrueType {};
        template <> struct TTypeIsIntegral<char> : TTrueType {};
#if defined(__cpp_char8_t)
        template <> struct TTypeIsIntegral<char8_t> : TTrueType {};
#endif
        template <> struct TTypeIsIntegral<char16_t> : TTrueType {};
        template <> struct TTypeIsIntegral<char32_t> : TTrueType {};
        template <> struct TTypeIsIntegral<wchar_t> : TTrueType {};
        template <> struct TTypeIsIntegral<signed char> : TTrueType {};
        template <> struct TTypeIsIntegral<unsigned char> : TTrueType {};
        template <> struct TTypeIsIntegral<short> : TTrueType {};
        template <> struct TTypeIsIntegral<unsigned short> : TTrueType {};
        template <> struct TTypeIsIntegral<int> : TTrueType {};
        template <> struct TTypeIsIntegral<unsigned int> : TTrueType {};
        template <> struct TTypeIsIntegral<long> : TTrueType {};
        template <> struct TTypeIsIntegral<unsigned long> : TTrueType {};
        template <> struct TTypeIsIntegral<long long> : TTrueType {};
        template <> struct TTypeIsIntegral<unsigned long long> : TTrueType {};

        template <typename> struct TTypeIsFloatingPoint : TFalseType {};
        template <> struct TTypeIsFloatingPoint<float> : TTrueType {};
        template <> struct TTypeIsFloatingPoint<double> : TTrueType {};
        template <> struct TTypeIsFloatingPoint<long double> : TTrueType {};

    } // namespace Detail::BasicTypes

    template <typename T>
    struct TTypeIsIntegral : Detail::BasicTypes::TTypeIsIntegral<typename TRemoveCV<T>::TType> {};
    template <typename T>
    struct TTypeIsFloatingPoint :
        Detail::BasicTypes::TTypeIsFloatingPoint<typename TRemoveCV<T>::TType> {};

    template <typename T, bool = TTypeIsIntegral<T>::Value> struct TTypeIsSigned : TFalseType {};
    template <typename T>
    struct TTypeIsSigned<T, true> : TBoolConstant<(static_cast<T>(-1) < static_cast<T>(0))> {};

    template <typename T>
    concept CCharType = CSameAs<T, char>
#if defined(__cpp_char8_t)
        || CSameAs<T, char8_t>
#endif
        || CSameAs<T, char16_t> || CSameAs<T, char32_t> || CSameAs<T, wchar_t>;

    template <typename T>
    concept CScalar = TTypeIsIntegral<T>::Value || TTypeIsFloatingPoint<T>::Value;
    template <typename T>
    concept CIntegral = TTypeIsIntegral<T>::Value;
    template <typename T>
    concept CFloatingPoint = TTypeIsFloatingPoint<T>::Value;
    template <typename T>
    concept CSignedIntegral = TTypeIsIntegral<T>::Value && TTypeIsSigned<T>::Value;

    // Constructible

    namespace Detail::Construction {
        template <typename T, typename... Args>
        auto TestConstructParen(int) -> decltype(T(Declval<Args>()...), TTrueType{});

        template <typename T, typename... Args> TFalseType TestConstructParen(...);

        template <typename T, typename... Args>
        auto TestConstructBrace(int) -> decltype(T{ Declval<Args>()... }, TTrueType{});

        template <typename T, typename... Args> TFalseType TestConstructBrace(...);
    } // namespace Detail::Construction

    template <typename T, typename... Args>
    struct TTypeIsConstructible :
        TBoolConstant<decltype(Detail::Construction::TestConstructParen<T, Args...>(0))::Value
            || decltype(Detail::Construction::TestConstructBrace<T, Args...>(0))::Value> {};

    template <typename T> struct TTypeIsDefaultConstructible : TTypeIsConstructible<T> {};
    template <typename T>
    struct TTypeIsTriviallyConstructible :
        Detail::CompilerTraits::TTypeIsTriviallyConstructibleImpl<T> {};

    template <typename T> struct TTypeIsCopyConstructible : TTypeIsConstructible<T, const T&> {};
    template <typename T> struct TTypeIsMoveConstructible : TTypeIsConstructible<T, T&&> {};

    template <typename T>
    concept CDefaultConstructible = TTypeIsDefaultConstructible<T>::Value;
    template <typename T>
    concept CTriviallyConstructible = TTypeIsTriviallyConstructible<T>::Value;
    template <typename T>
    concept CCopyConstructible = TTypeIsCopyConstructible<T>::Value;
    template <typename T>
    concept CMoveConstructible = TTypeIsMoveConstructible<T>::Value;

    // Destructible
    template <typename T>
    concept CDestructible = requires(T& obj) {
        { obj.~T() };
    };

    // Conversion
    template <typename TFrom, typename TTo>
    concept CStaticConvertible = requires(TFrom obj) {
        { static_cast<TTo>(obj) } -> CSameAs<TTo>;
    };

    template <typename TFrom, typename TTo>
    concept CDynamicConvertible = requires(TFrom obj) {
        { dynamic_cast<TTo>(obj) } -> CSameAs<TTo>;
    };

    // Class Type Extraction
    template <typename T> struct TMemberType {
        using TBaseType = T;
    };
    template <typename U, typename T> struct TMemberType<U T::*> {
        using TBaseType  = U;
        using TClassType = T;
    };
    template <typename> struct TMemberFunctionTrait : TTrueType {};

    template <typename R, typename C, typename... Args>
    struct TMemberFunctionTrait<R (C::*)(Args...)> : TFalseType {
        using TReturnType = R;
        using TClassType  = C;
        using TArgsTuple  = Core::Container::TTuple<Args...>;
    };
    template <typename T>
    concept CMemberPointer = requires(T t) {
        typename TMemberType<T>::TBaseType;
        typename TMemberType<T>::TClassType;
    };

    template <typename T>
    concept CMemberFunctionPointer = requires(T t) {
        typename TMemberFunctionTrait<T>::TReturnType;
        typename TMemberFunctionTrait<T>::TClassType;
        typename TMemberFunctionTrait<T>::TArgsTuple;
    };

    // Inheritance / Polymorphism
    namespace Detail::Inheritance {
        template <typename B> auto TestPtrConv(const volatile B*) -> TTrueType;
        template <typename> auto   TestPtrConv(const volatile void*) -> TFalseType;

        template <typename B, typename D>
        auto TestIsBaseOf(int) -> decltype(TestPtrConv<B>(static_cast<D*>(nullptr)));
        template <typename, typename> auto TestIsBaseOf(...) -> TTrueType;
    } // namespace Detail::Inheritance

    template <typename TBase, typename TDerived>
    struct TTypeIsBaseOf :
        TBoolConstant<TTypeIsClass<TBase>::Value
            && TTypeIsClass<TDerived>::Value&& decltype(Detail::Inheritance::TestIsBaseOf<TBase,
                TDerived>(0))::Value> {};

    template <typename TBase, typename TDerived>
    concept CClassBaseOf = TTypeIsBaseOf<TBase, TDerived>::Value;

    // Comparators
    template <typename T, typename U = T>
    concept CLessComparable = requires(T a, U b) {
        { a < b } -> CStaticConvertible<bool>;
    };
    template <typename T, typename U = T>
    concept CGreaterComparable = requires(T a, U b) {
        { a > b } -> CStaticConvertible<bool>;
    };
    template <typename T, typename U = T>
    concept CEqualComparable = requires(T a, U b) {
        { a == b } -> CStaticConvertible<bool>;
    };

    // Operators
    template <typename It>
    concept CIncrementable = requires(It it) {
        { ++it };
        { it++ };
    };

    // Accessor
    template <typename T>
    concept CRandomReadable = requires(const T& t) {
        { t[usize{}] };
    };
    template <typename T>
    concept CRandomWritable = requires(T& t) {
        { t[usize{}] };
    };

    // Iterator
    template <typename It>
    concept CReadableIterator = requires(It& it) {
        *it;
        ++it;
    };
    template <typename It>
    concept CWritableIterator = requires(It& it) {
        *it = *it;
        ++it;
    };
    template <typename It>
    concept CRandomAccessIterator = requires(It& it, It other, usize n) {
        { it + n } -> CSameAs<It>;
        { it - n } -> CSameAs<It>;
        { it += n } -> CSameAs<It&>;
        { it -= n } -> CSameAs<It&>;
        it[n];
        { other - it };
    };

    // Range
    template <typename R, typename Pred>
    concept CPredicateForRange = requires(Pred p, decltype(Declval<R>().begin()) it) {
        { p(*it) };
        { static_cast<bool>(p(*it)) };
    };

    template <typename R>
    concept CRange = requires(R r) {
        { r.begin() };
        { r.end() };
    };

    template <typename R>
    concept CCommonRange =
        CRange<R> && CSameAs<decltype(Declval<R>().begin()), decltype(Declval<R>().end())>;

    template <typename R>
    concept CReadableRange = CRange<R> && CReadableIterator<decltype(Declval<R>().begin())>;

    template <typename R>
    concept CWritableRange = CRange<R> && CWritableIterator<decltype(Declval<R>().begin())>;

    template <typename R>
    concept CForwardRange = CRange<R> && CIncrementable<decltype(Declval<R>().begin())>
        && CReadableIterator<decltype(Declval<R>().begin())>;

    // Compile-time Utility
    constexpr auto IsConstantEvaluated() noexcept -> bool {
        if consteval {
            return true;
        }
        return false;
    }

    // Comparator Wrappers
    template <typename T = void> struct TLess {
        constexpr auto operator()(const T& a, const T& b) const noexcept(noexcept(a < b)) -> bool {
            return a < b;
        }
    };

    template <> struct TLess<void> {
        template <typename L, typename R>
        constexpr auto operator()(L&& l, R&& r) const -> decltype(Declval<L>() < Declval<R>()) {
            return static_cast<L&&>(l) < static_cast<R&&>(r);
        }
    };

    template <typename T = void> struct TGreater {
        constexpr auto operator()(const T& a, const T& b) const noexcept(noexcept(a > b)) -> bool {
            return a > b;
        }
    };

    template <> struct TGreater<void> {
        template <typename L, typename R>
        constexpr auto operator()(L&& l, R&& r) const -> decltype(Declval<L>() > Declval<R>()) {
            return static_cast<L&&>(l) > static_cast<R&&>(r);
        }
    };

    template <typename T = void> struct TEqual {
        constexpr auto operator()(const T& a, const T& b) const noexcept(noexcept(a == b)) -> bool {
            return a == b;
        }
    };

    template <> struct TEqual<void> {
        template <typename L, typename R>
        constexpr auto operator()(L&& l, R&& r) const -> decltype(Declval<L>() == Declval<R>()) {
            return static_cast<L&&>(l) == static_cast<R&&>(r);
        }
    };

} // namespace AltinaEngine
