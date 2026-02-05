#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <functional>
#include <iostream>
#include <cmath>
#include <cstdlib>

namespace Test {

    using TestFunc = void (*)();

    struct Case {
        const char* name;
        TestFunc    func;
    };

    inline std::vector<Case>& cases() {
        static std::vector<Case> v;
        return v;
    }

    inline int current_checks   = 0;
    inline int current_failures = 0;

    struct Registrar {
        Registrar(const char* name, TestFunc f) { cases().push_back({ name, f }); }
    };

    inline int run_all() {
        int total_failures = 0;
        const char* filter = std::getenv("ALTINA_TEST_FILTER");
        const char* start  = std::getenv("ALTINA_TEST_START");
        const char* stop   = std::getenv("ALTINA_TEST_STOP_AFTER");
        const char* list   = std::getenv("ALTINA_TEST_LIST");
        bool        started =
            (start == nullptr) || (start[0] == '\0');
        if ((list != nullptr) && (list[0] != '\0')) {
            for (auto& c : cases()) {
                std::cout << c.name << std::endl;
            }
            return 0;
        }
        std::cout << "Running " << cases().size() << " test(s)" << std::endl;
        for (auto& c : cases()) {
            if (!started) {
                if (std::string_view(c.name).find(start) != std::string_view::npos) {
                    started = true;
                } else {
                    continue;
                }
            }
            if ((filter != nullptr) && (filter[0] != '\0')) {
                if (std::string_view(c.name).find(filter) == std::string_view::npos) {
                    continue;
                }
            }
            current_checks   = 0;
            current_failures = 0;
            std::cout << "[ RUN ] " << c.name << std::endl;
            try {
                c.func();
            } catch (const std::exception& e) {
                std::cerr << "Unhandled exception in " << c.name << ": " << e.what() << '\n';
                current_failures++;
            } catch (...) {
                std::cerr << "Unhandled non-standard exception in " << c.name << '\n';
                current_failures++;
            }
            std::cout << "[ DEBUG ] completed " << c.name << " checks=" << current_checks
                      << " failures=" << current_failures << std::endl;
            if (current_failures == 0)
                std::cout << "[  OK  ] " << c.name << std::endl;
            else
                std::cout << "[FAILED] " << c.name << " (" << current_failures << " failed checks)"
                          << std::endl;

            total_failures += current_failures;

            if ((stop != nullptr) && (stop[0] != '\0')) {
                if (std::string_view(c.name).find(stop) != std::string_view::npos) {
                    break;
                }
            }
        }
        return total_failures;
    }

// Helper macros
#define TEST_CONCAT2(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT2(a, b)

#define TEST_CASE(name)                                       \
    static void            TEST_CONCAT(test_fn_, __LINE__)(); \
    static Test::Registrar TEST_CONCAT(test_reg_, __LINE__)(  \
        name, &TEST_CONCAT(test_fn_, __LINE__));              \
    static void TEST_CONCAT(test_fn_, __LINE__)()

#define STATIC_REQUIRE(x) static_assert((x))

#define REQUIRE(expr) Test::Require((expr), #expr, __FILE__, __LINE__)

#define REQUIRE_EQ(a, b) REQUIRE((a) == (b))

#define REQUIRE_CLOSE(a, b, eps) Test::RequireClose((a), (b), (eps), #a, #b, __FILE__, __LINE__)

    // Inline helpers replace previous do/while(0) macros to satisfy
    // cppcoreguidelines-avoid-do-while while preserving diagnostics.
    template <typename T>
    inline void Require(T&& expr, const char* exprText, const char* file, int line) {
        ++current_checks;
        if (!static_cast<bool>(expr)) {
            ++current_failures;
            std::cerr << "FAIL: " << file << ":" << line << " - " << exprText << '\n';
        }
    }

    template <typename T, typename U, typename E>
    inline void RequireClose(
        T a, U b, E eps, const char* aText, const char* bText, const char* file, int line) {
        ++current_checks;
        if (std::fabs((double)a - (double)b) > (double)eps) {
            ++current_failures;
            std::cerr << "FAIL: " << file << ":" << line << " - close(" << aText << "," << bText
                      << ")" << '\n';
        }
    }

} // namespace Test
