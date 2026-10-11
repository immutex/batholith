#pragma once

#include <cmath>
#include <cstdio>
#include <string_view>

namespace batholith::test {

inline int failures = 0;
inline int checks = 0;

inline void report(bool ok, std::string_view expression, std::string_view file, int line) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("FAIL %s:%d  %s\n", file.data(), line, expression.data());
    }
}

inline void report_near(double actual, double expected, double tolerance,
                        std::string_view expression, std::string_view file, int line) {
    ++checks;
    if (!(std::fabs(actual - expected) <= tolerance)) {
        ++failures;
        std::printf("FAIL %s:%d  %s  (actual=%.17g expected=%.17g)\n", file.data(), line,
                    expression.data(), actual, expected);
    }
}

inline int summary() {
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

}  // namespace batholith::test

#define CHECK(expr) ::batholith::test::report((expr), #expr, __FILE__, __LINE__)
// The widening to double is deliberate: comparisons happen at double precision even when
// the values under test are float, and stating it here keeps callers free of casts.
#define CHECK_NEAR(actual, expected, tolerance) \
    ::batholith::test::report_near(static_cast<double>(actual), static_cast<double>(expected), \
                                  static_cast<double>(tolerance), #actual, __FILE__, __LINE__)
