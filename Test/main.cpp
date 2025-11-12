#include <iostream>

#include "Types/Traits.h"
#include "Types/Aliases.h"

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

    std::cout << "All tests passed\n";
    return 0;
}
