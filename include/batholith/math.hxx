#pragma once

#include <cmath>
#include <cstdint>

namespace batholith {

[[nodiscard]] constexpr double lerp(double delta, double start, double end) noexcept {
    return start + delta * (end - start);
}

[[nodiscard]] constexpr double clamped_lerp(double start, double end, double delta) noexcept {
    return delta < 0.0 ? start : (delta > 1.0 ? end : start + delta * (end - start));
}

template <typename T>
[[nodiscard]] constexpr T clamp(T value, T low, T high) noexcept {
    return value < low ? low : (value > high ? high : value);
}

[[nodiscard]] constexpr int floor_div(int value, int divisor) noexcept {
    const int quotient = value / divisor;
    return (value % divisor != 0 && ((value < 0) != (divisor < 0))) ? quotient - 1 : quotient;
}

[[nodiscard]] inline int floor_to_int(double value) noexcept {
    const int truncated = static_cast<int>(value);
    return value < static_cast<double>(truncated) ? truncated - 1 : truncated;
}

}  // namespace batholith
