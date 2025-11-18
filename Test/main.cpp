#include <iostream>

#include "Types/Traits.h"
#include "Types/Aliases.h"
#include "Math/Vector.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#if AE_PLATFORM_WIN
    #include "Platform/Windows/PlatformIntrinsicWindows.h"
#endif

using namespace AltinaEngine;

int main()
{
    // compile-time checks
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

    // runtime checks for comparator objects
    if (!TLess<>{}(3, 4))
    {
        std::cerr << "less failed\n";
        return 2;
    }
    if (!TGreater<int>{}(5, 2))
    {
        std::cerr << "greater failed\n";
        return 3;
    }
    if (!TEqual<int>{}(7, 7))
    {
        std::cerr << "equal failed\n";
        return 4;
    }

    {
        using namespace AltinaEngine::Core::Math;

        FVector3f       VecA{ { 1.0F, 2.0F, 3.0F } };
        const FVector3f VecB{ { 4.0F, 5.0F, 6.0F } };

        const auto      VecAdd = VecA + VecB;
        if (VecAdd[0U] != 5.0F || VecAdd[1U] != 7.0F || VecAdd[2U] != 9.0F)
        {
            std::cerr << "Vector + failed\n";
            return 11;
        }

        const auto VecSub = VecB - VecA;
        if (VecSub[0U] != 3.0F || VecSub[1U] != 3.0F || VecSub[2U] != 3.0F)
        {
            std::cerr << "Vector - failed\n";
            return 12;
        }

        const FVector3f VecC{ { 2.0F, 3.0F, 4.0F } };
        const auto      VecMul = VecA * VecC;
        if (VecMul[0U] != 2.0F || VecMul[1U] != 6.0F || VecMul[2U] != 12.0F)
        {
            std::cerr << "Vector * failed\n";
            return 13;
        }

        const FVector3f VecD{ { 2.0F, 4.0F, 8.0F } };
        const FVector3f VecDivisor{ { 2.0F, 2.0F, 4.0F } };
        const auto      VecDiv = VecD / VecDivisor;
        if (VecDiv[0U] != 1.0F || VecDiv[1U] != 2.0F || VecDiv[2U] != 2.0F)
        {
            std::cerr << "Vector / failed\n";
            return 14;
        }

        FVector3i       RuntimeA{ { 1, 2, 3 } };
        const FVector3i RuntimeB{ { 4, 5, 6 } };
        RuntimeA += RuntimeB;
        if (RuntimeA[0U] != 5 || RuntimeA[1U] != 7 || RuntimeA[2U] != 9)
        {
            std::cerr << "Vector += failed\n";
            return 15;
        }

        RuntimeA -= RuntimeB;
        if (RuntimeA[0U] != 1 || RuntimeA[1U] != 2 || RuntimeA[2U] != 3)
        {
            std::cerr << "Vector -= failed\n";
            return 16;
        }

        const FVector3i Scale{ { 2, 3, 4 } };
        RuntimeA *= Scale;
        if (RuntimeA[0U] != 2 || RuntimeA[1U] != 6 || RuntimeA[2U] != 12)
        {
            std::cerr << "Vector *= failed\n";
            return 17;
        }

        RuntimeA /= Scale;
        if (RuntimeA[0U] != 1 || RuntimeA[1U] != 2 || RuntimeA[2U] != 3)
        {
            std::cerr << "Vector /= failed\n";
            return 18;
        }
    }

#if AE_PLATFORM_WIN
    using namespace AltinaEngine::Core::Platform;

    // Compile-time validation of Windows-specific intrinsics
    static_assert(PopCount32(0xFFFF0000U) == 16U);
    static_assert(PopCount64(0xFFFF0000FFFF0000ULL) == 32U);
    static_assert(CountLeadingZeros32(1U) == 31U);
    static_assert(CountLeadingZeros64(1ULL) == 63U);
    static_assert(CountTrailingZeros32(1U << 12U) == 12U);
    static_assert(CountTrailingZeros64(1ULL << 36U) == 36U);

    // Runtime validation paths (exercise intrinsic-backed implementations)
    if (PopCount32(0xF0F0F0F0U) != 16U)
    {
        std::cerr << "PopCount32 runtime failed\n";
        return 5;
    }
    if (PopCount64(0xF0F0F0F0F0F0F0F0ULL) != 32U)
    {
        std::cerr << "PopCount64 runtime failed\n";
        return 6;
    }
    if (CountLeadingZeros32(0x04000000U) != 5U)
    {
        std::cerr << "CountLeadingZeros32 runtime failed\n";
        return 7;
    }
    if (CountLeadingZeros64(1ULL << 40U) != 23U)
    {
        std::cerr << "CountLeadingZeros64 runtime failed\n";
        return 8;
    }
    if (CountTrailingZeros32(0x02000000U) != 25U)
    {
        std::cerr << "CountTrailingZeros32 runtime failed\n";
        return 9;
    }
    if (CountTrailingZeros64(1ULL << 42U) != 42U)
    {
        std::cerr << "CountTrailingZeros64 runtime failed\n";
        return 10;
    }
#endif // AE_PLATFORM_WIN

    std::cout << "All tests passed\n";
    return 0;
}
