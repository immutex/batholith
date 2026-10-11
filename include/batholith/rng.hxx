#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace batholith {

class xoroshiro128pp {
public:
    xoroshiro128pp() = default;
    xoroshiro128pp(std::uint64_t lo, std::uint64_t hi) noexcept;

    static xoroshiro128pp from_seed(std::uint64_t seed) noexcept;

    [[nodiscard]] std::uint64_t next_long() noexcept;
    [[nodiscard]] std::int32_t next_int() noexcept;
    [[nodiscard]] std::int32_t next_int(std::int32_t bound) noexcept;
    [[nodiscard]] double next_double() noexcept;
    [[nodiscard]] float next_float() noexcept;

    void consume(std::int32_t count) noexcept;

    [[nodiscard]] std::uint64_t lo() const noexcept { return lo_; }
    [[nodiscard]] std::uint64_t hi() const noexcept { return hi_; }

private:
    [[nodiscard]] static std::uint64_t mix_stafford_13(std::uint64_t value) noexcept;
    [[nodiscard]] static std::uint64_t rotl(std::uint64_t value, int shift) noexcept;

    std::uint64_t lo_ = 0;
    std::uint64_t hi_ = 0;
};

class positional_factory {
public:
    positional_factory() = default;
    positional_factory(std::uint64_t lo, std::uint64_t hi) noexcept : lo_(lo), hi_(hi) {}

    [[nodiscard]] xoroshiro128pp at(int x, int y, int z) const noexcept;
    [[nodiscard]] xoroshiro128pp from_hash_of(std::string_view name) const noexcept;

private:
    std::uint64_t lo_ = 0;
    std::uint64_t hi_ = 0;
};

[[nodiscard]] std::int64_t block_seed(int x, int y, int z) noexcept;
[[nodiscard]] std::array<std::uint64_t, 2> md5_seed(std::string_view text) noexcept;

}  // namespace batholith
