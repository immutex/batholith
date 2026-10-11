#include <batholith/rng.hxx>

#include <bit>
#include <cstring>

namespace batholith {
namespace {

constexpr std::uint64_t golden_ratio_64 = 0x9E3779B97F4A7C15ULL;
constexpr std::uint64_t silver_ratio_64 = 0x6A09E667F3BCC909ULL;
constexpr std::uint64_t stafford_a = 0xBF58476D1CE4E5B9ULL;
constexpr std::uint64_t stafford_b = 0x94D049BB133111EBULL;
constexpr std::uint64_t double_unit_shift = 11;
constexpr double double_unit = 0x1.0p-53;
constexpr std::uint64_t float_unit_shift = 40;
constexpr float float_unit = 0x1.0p-24F;

constexpr std::uint32_t md5_rotations[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9,  14, 20, 5, 9,
    14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

constexpr std::uint32_t md5_sines[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613,
    0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193,
    0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d,
    0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122,
    0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
    0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244,
    0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb,
    0xeb86d391};

[[nodiscard]] std::uint32_t rotl32(std::uint32_t value, std::uint32_t shift) noexcept {
    return (value << shift) | (value >> (32U - shift));
}

void md5_digest(std::string_view text, std::uint8_t out[16]) noexcept {
    const std::size_t padded = ((text.size() + 8) / 64 + 1) * 64;
    std::vector<std::uint8_t> buffer(padded, 0);
    std::memcpy(buffer.data(), text.data(), text.size());
    buffer[text.size()] = 0x80;
    const std::uint64_t bit_length = static_cast<std::uint64_t>(text.size()) * 8;
    for (int index = 0; index < 8; ++index) {
        buffer[padded - 8 + static_cast<std::size_t>(index)] =
            static_cast<std::uint8_t>(bit_length >> (8 * index));
    }

    std::uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
    for (std::size_t chunk = 0; chunk < padded; chunk += 64) {
        std::uint32_t words[16];
        for (int index = 0; index < 16; ++index) {
            const std::size_t base = chunk + static_cast<std::size_t>(index) * 4;
            words[index] = static_cast<std::uint32_t>(buffer[base]) |
                           (static_cast<std::uint32_t>(buffer[base + 1]) << 8) |
                           (static_cast<std::uint32_t>(buffer[base + 2]) << 16) |
                           (static_cast<std::uint32_t>(buffer[base + 3]) << 24);
        }

        std::uint32_t a = a0, b = b0, c = c0, d = d0;
        for (std::uint32_t step = 0; step < 64; ++step) {
            std::uint32_t f = 0;
            std::uint32_t g = 0;
            if (step < 16) {
                f = (b & c) | (~b & d);
                g = step;
            } else if (step < 32) {
                f = (d & b) | (~d & c);
                g = (5 * step + 1) % 16;
            } else if (step < 48) {
                f = b ^ c ^ d;
                g = (3 * step + 5) % 16;
            } else {
                f = c ^ (b | ~d);
                g = (7 * step) % 16;
            }
            f += a + md5_sines[step] + words[g];
            a = d;
            d = c;
            c = b;
            b += rotl32(f, md5_rotations[step]);
        }
        a0 += a;
        b0 += b;
        c0 += c;
        d0 += d;
    }

    const std::uint32_t words[4] = {a0, b0, c0, d0};
    for (int index = 0; index < 4; ++index) {
        for (int byte = 0; byte < 4; ++byte) {
            out[index * 4 + byte] =
                static_cast<std::uint8_t>(words[index] >> (8 * byte));
        }
    }
}

}  // namespace

std::uint64_t xoroshiro128pp::rotl(std::uint64_t value, int shift) noexcept {
    return (value << shift) | (value >> (64 - shift));
}

std::uint64_t xoroshiro128pp::mix_stafford_13(std::uint64_t value) noexcept {
    value = (value ^ (value >> 30)) * stafford_a;
    value = (value ^ (value >> 27)) * stafford_b;
    return value ^ (value >> 31);
}

xoroshiro128pp::xoroshiro128pp(std::uint64_t lo, std::uint64_t hi) noexcept : lo_(lo), hi_(hi) {
    if ((lo_ | hi_) == 0) {
        lo_ = golden_ratio_64;
        hi_ = silver_ratio_64;
    }
}

xoroshiro128pp xoroshiro128pp::from_seed(std::uint64_t seed) noexcept {
    const std::uint64_t low = seed ^ silver_ratio_64;
    const std::uint64_t high = low + golden_ratio_64;
    return {mix_stafford_13(low), mix_stafford_13(high)};
}

std::uint64_t xoroshiro128pp::next_long() noexcept {
    const std::uint64_t lo = lo_;
    const std::uint64_t hi = hi_;
    const std::uint64_t result = rotl(lo + hi, 17) + lo;
    const std::uint64_t mixed = hi ^ lo;
    lo_ = rotl(lo, 49) ^ mixed ^ (mixed << 21);
    hi_ = rotl(mixed, 28);
    return result;
}

std::int32_t xoroshiro128pp::next_int() noexcept {
    return static_cast<std::int32_t>(next_long());
}

std::int32_t xoroshiro128pp::next_int(std::int32_t bound) noexcept {
    const std::uint64_t limit = static_cast<std::uint64_t>(bound);
    std::uint64_t candidate = static_cast<std::uint32_t>(next_int());
    std::uint64_t product = candidate * limit;
    std::uint64_t low = static_cast<std::uint32_t>(product);
    if (low < limit) {
        const std::uint64_t threshold =
            static_cast<std::uint64_t>(-static_cast<std::int64_t>(limit)) % limit;
        while (low < threshold) {
            candidate = static_cast<std::uint32_t>(next_int());
            product = candidate * limit;
            low = static_cast<std::uint32_t>(product);
        }
    }
    return static_cast<std::int32_t>(product >> 32);
}

double xoroshiro128pp::next_double() noexcept {
    return static_cast<double>(next_long() >> double_unit_shift) * double_unit;
}

float xoroshiro128pp::next_float() noexcept {
    return static_cast<float>(next_long() >> float_unit_shift) * float_unit;
}

void xoroshiro128pp::consume(std::int32_t count) noexcept {
    for (std::int32_t index = 0; index < count; ++index) {
        static_cast<void>(next_int());
    }
}

std::int64_t block_seed(int x, int y, int z) noexcept {
    const std::int32_t mixed_x = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(x) * 3129871U);
    std::int64_t value = static_cast<std::int64_t>(mixed_x) ^
                         (static_cast<std::int64_t>(z) * 116129781LL) ^
                         static_cast<std::int64_t>(y);
    value = value * value * 42317861LL + value * 11LL;
    return value >> 16;
}

std::array<std::uint64_t, 2> md5_seed(std::string_view text) noexcept {
    std::uint8_t digest[16];
    md5_digest(text, digest);
    std::uint64_t lo = 0;
    std::uint64_t hi = 0;
    for (int index = 0; index < 8; ++index) {
        lo = (lo << 8) | digest[index];
        hi = (hi << 8) | digest[8 + index];
    }
    return {lo, hi};
}

xoroshiro128pp positional_factory::at(int x, int y, int z) const noexcept {
    return {static_cast<std::uint64_t>(block_seed(x, y, z)) ^ lo_, hi_};
}

xoroshiro128pp positional_factory::from_hash_of(std::string_view name) const noexcept {
    const auto [hash_lo, hash_hi] = md5_seed(name);
    return {hash_lo ^ lo_, hash_hi ^ hi_};
}

}  // namespace batholith
