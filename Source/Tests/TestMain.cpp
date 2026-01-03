#include <iostream>
#include "TestHarness.h"

int main() {
    const int failures = Test::run_all();
    if (failures == 0) {
        std::cout << "All tests passed\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
