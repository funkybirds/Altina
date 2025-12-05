// Compile-time checks only. Keeping these in their own TU ensures static_asserts
// are compiled even if runtime tests are disabled.

#include "Types/Traits.h"

using namespace AltinaEngine;

static_assert(TTypeIsIntegral_v<int>);
static_assert(TTypeIsIntegral_v<unsigned long long>);
static_assert(!TTypeIsIntegral_v<float>);
static_assert(TTypeIsFloatingPoint_v<double>);

static_assert(TTypeIsDefaultConstructible_v<int>);
static_assert(TTypeIsCopyConstructible_v<int>);
static_assert(TTypeIsMovable_v<int>);

static_assert(TTypeLessComparable_v<int>);
static_assert(TTypeEqualComparable_v<int>);
static_assert(TTypeGreaterComparable_v<int>);
