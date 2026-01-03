// Compile-time checks only. Keeping these in their own TU ensures static_asserts
// are compiled even if runtime tests are disabled.

#include "Types/Traits.h"

using namespace AltinaEngine;

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
