#pragma once

#include "Base/AltinaBase.h"
#include "Reflection/Reflection.h"
#include "Container/Ref.h"
#include "Types/Meta.h"

using namespace AltinaEngine::Core;
using namespace AltinaEngine::Core::Reflection;
using namespace AltinaEngine::Core::Container;
using namespace AltinaEngine::Core::TypeMeta;
using AltinaEngine::u64;

// Test type definitions - shared across all reflection tests
struct FSimpleTestStruct
{
    int    mIntValue    = 42;
    float  mFloatValue  = 3.14f;
    double mDoubleValue = 2.718;
};

struct FNestedTestStruct
{
    int               mId = 100;
    FSimpleTestStruct mNested;
};

struct FPolymorphicBase
{
    virtual ~FPolymorphicBase() = default;
    int mBaseValue              = 10;
};

struct FPolymorphicDerived : FPolymorphicBase
{
    int mDerivedValue = 20;
};

struct FComplexStruct
{
    int    mA = 1;
    int    mB = 2;
    int    mC = 3;
    float  mX = 1.0f;
    float  mY = 2.0f;
    double mZ = 3.0;
};

struct FLargeStruct
{
    int    mValues[100];
    double mDoubles[50];

    FLargeStruct()
    {
        for (int i = 0; i < 100; ++i)
            mValues[i] = i;
        for (int i = 0; i < 50; ++i)
            mDoubles[i] = i * 0.5;
    }
};

struct FEmptyStruct
{
    // Intentionally empty
};

namespace ReflectionTestHelpers
{
    // Thread-safe, one-time registration for all reflection test types
    // Declaration only - implementation in .cpp file
    void EnsureTypesRegistered();

} // namespace ReflectionTestHelpers
