#pragma once

#include <vector>
#include <string>
#include <functional>
#include <iostream>
#include <cmath>

namespace Test {

using TestFunc = void(*)();

struct Case { const char* name; TestFunc func; };

inline std::vector<Case>& cases()
{
    static std::vector<Case> v;
    return v;
}

inline int current_checks = 0;
inline int current_failures = 0;

struct Registrar {
    Registrar(const char* name, TestFunc f)
    {
        cases().push_back({ name, f });
    }
};

inline int run_all()
{
    int total_failures = 0;
    std::cout << "Running " << cases().size() << " test(s)\n";
    for (auto &c : cases())
    {
        current_checks = 0;
        current_failures = 0;
        std::cout << "[ RUN ] " << c.name << std::endl;
        try {
            c.func();
        } catch (const std::exception &e) {
            std::cerr << "Unhandled exception in " << c.name << ": " << e.what() << std::endl;
            current_failures++;
        } catch (...) {
            std::cerr << "Unhandled non-standard exception in " << c.name << std::endl;
            current_failures++;
        }
        if (current_failures == 0)
            std::cout << "[  OK  ] " << c.name << std::endl;
        else
            std::cout << "[FAILED] " << c.name << " (" << current_failures << " failed checks)" << std::endl;

        total_failures += current_failures;
    }
    return total_failures;
}

// Helper macros
#define TEST_CONCAT2(a,b) a##b
#define TEST_CONCAT(a,b) TEST_CONCAT2(a,b)

#define TEST_CASE(name) \
    static void TEST_CONCAT(test_fn_, __LINE__)(); \
    static Test::Registrar TEST_CONCAT(test_reg_, __LINE__)(name, &TEST_CONCAT(test_fn_, __LINE__)); \
    static void TEST_CONCAT(test_fn_, __LINE__)()

#define STATIC_REQUIRE(x) static_assert(x)

#define REQUIRE(expr) do { \
    ++Test::current_checks; \
    if (!(expr)) { \
        ++Test::current_failures; \
        std::cerr << "FAIL: " << __FILE__ << ":" << __LINE__ << " - " #expr << std::endl; \
    } \
} while(0)

#define REQUIRE_EQ(a,b) REQUIRE((a)==(b))

#define REQUIRE_CLOSE(a,b,eps) do { \
    ++Test::current_checks; \
    if (std::fabs((double)(a) - (double)(b)) > (double)(eps)) { \
        ++Test::current_failures; \
        std::cerr << "FAIL: " << __FILE__ << ":" << __LINE__ << " - close(" #a "," #b ")" << std::endl; \
    } \
} while(0)

} // namespace Test
