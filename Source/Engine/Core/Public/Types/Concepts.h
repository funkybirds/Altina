#pragma once

#include "Traits.h"

namespace AltinaEngine {
    // Predicates
    template <typename T>
    concept CDefinedType = requires { sizeof(T); };

    template <typename T, typename U>
    concept CSameAs = TTypeSameAs<T, U>::Value;

    template <typename T, typename U>
    concept CSameSizeAs = sizeof(T) == sizeof(U);

    template <typename T, typename... Ts>
    concept CSameAsAll = (sizeof...(Ts) == 0) || (TTypeSameAs<T, Ts>::Value && ...);

    // Basic Traits
    template <typename T>
    concept CCharType = TTypeSameAs_v<T, char>
#if defined(__cpp_char8_t)
        || TTypeSameAs_v<T, char8_t>
#endif
        || TTypeSameAs_v<T, char16_t> || TTypeSameAs_v<T, char32_t> || TTypeSameAs_v<T, wchar_t>;

    template <typename T>
    concept CScalar = TTypeIsIntegral<T>::Value || TTypeIsFloatingPoint<T>::Value;

    template <typename T>
    concept CNonVoid = !TTypeIsVoid_v<T>;

    template <typename T>
    concept CVoid = TTypeIsVoid_v<T>;

    template <typename T>
    concept CIntegral = TTypeIsIntegral<T>::Value;

    template <typename T>
    concept CFloatingPoint = TTypeIsFloatingPoint<T>::Value;

    template <typename T>
    concept CSignedIntegral = TTypeIsIntegral<T>::Value && TTypeIsSigned<T>::Value;

    template <typename T>
    concept CRandomReadable = TTypeIsRandomReadable<T>::Value;

    template <typename T>
    concept CRandomWritable = TTypeIsRandomWritable<T>::Value;

    template <typename From, typename To>
    concept CDynamicConvertible = TTypeIsDynamicConvertible<To, From>::Value;

    template <typename T>
    concept CDefaultConstructible = TTypeIsDefaultConstructible_v<T>;

    template <typename T>
    concept CTriviallyConstructible = TTypeIsTriviallyConstructible<T>::Value;

    template <typename T>
    concept CClass = TTypeIsClass_v<T>;

    template <typename T>
    concept CUnion = TTypeIsUnion<T>::Value;

    template <typename T>
    concept CEnum = TTypeIsEnum<T>::Value;

    template <typename TBase, typename TDerived>
    concept CClassBaseOf = TTypeIsBaseOf<TBase, TDerived>::Value;

    template <typename T>
    concept CDecayed = TTypeIsDecayed<T>::Value;

    // Iterators
    template <typename It>
    concept CReadableIterator = TTypeIsReadableIterator<It>::Value;

    template <typename It>
    concept CWritableIterator = TTypeIsWritableIterator<It>::Value;

    template <typename It>
    concept CRandomAccessIterator = TTypeIsRandomAccessIterator<It>::Value;

    // Range concepts
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
    concept CCommonRange = CRange<R>
        && TTypeSameAs<decltype(Declval<R>().begin()), decltype(Declval<R>().end())>::Value;

    template <typename R>
    concept CReadableRange = CRange<R> && CReadableIterator<decltype(Declval<R>().begin())>;

    template <typename R>
    concept CWritableRange = CRange<R> && CWritableIterator<decltype(Declval<R>().begin())>;

    template <typename It>
    concept CIncrementable = requires(It it) {
        { ++it };
        { it++ };
    };

    template <typename R>
    concept CForwardRange = CRange<R> && CIncrementable<decltype(Declval<R>().begin())>
        && CReadableIterator<decltype(Declval<R>().begin())>;

    // Extractor Concepts
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

    // Constructor Concepts
    template <typename T>
    concept CCopyConstructible = TTypeIsCopyConstructible_v<T>;

} // namespace AltinaEngine
