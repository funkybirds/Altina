// Compile-time checks only. Keeping these in their own TU ensures static_asserts
// are compiled even if runtime tests are disabled.

#include "Types/Traits.h"

using namespace AltinaEngine;

namespace {
    template <typename T> struct TDependentTypeProbe {
        using TWithoutCv  = typename TRemoveCV<T>::TType;
        using TNormalized = typename TDecay<T>::TType;
        using TMoved      = TValueOrReferenceReturn<TNormalized>;
    };
} // namespace

static_assert(CIntegral<int>);
static_assert(CIntegral<unsigned long long>);
static_assert(!CIntegral<float>);
static_assert(CFloatingPoint<double>);

static_assert(CDefaultConstructible<int>);
static_assert(CCopyConstructible<int>);
static_assert(CMoveConstructible<int>);

static_assert(CLessComparable<int>);
static_assert(CEqualComparable<int>);
static_assert(CGreaterComparable<int>);

static_assert(CSameAs<typename TDependentTypeProbe<const int&>::TWithoutCv, const int&>);
static_assert(CSameAs<typename TDependentTypeProbe<const int&>::TNormalized, int>);
static_assert(CSameAs<TDependentTypeProbe<const int&>::TMoved, int>);
