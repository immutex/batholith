#include "assert.hxx"

#include <array>
#include <span>

#include <batholith/noise.hxx>
#include <batholith/rng.hxx>

namespace batholith::test {
namespace {

}  // namespace

void run_noise() {
    {
        const xoroshiro128pp first = xoroshiro128pp::from_seed(0);
        const xoroshiro128pp second = xoroshiro128pp::from_seed(0);
        CHECK(first.lo() == second.lo());
        CHECK(first.hi() == second.hi());
        CHECK(xoroshiro128pp(0, 0).lo() != 0);
    }

    {
        xoroshiro128pp a = xoroshiro128pp::from_seed(0);
        xoroshiro128pp b = xoroshiro128pp::from_seed(0);
        CHECK(a.next_long() == b.next_long());
        CHECK(a.next_double() == b.next_double());
        const double value = a.next_double();
        CHECK(value >= 0.0 && value < 1.0);
        const float small = a.next_float();
        CHECK(small >= 0.0F && small < 1.0F);
    }

    {
        const auto [lo, hi] = md5_seed("minecraft:terrain");
        CHECK(lo == 0x1EE555222EF96F14ULL);
        CHECK(hi == 0xE2BEDFDBEBE43D33ULL);
    }

    {
        const auto [lo, hi] = md5_seed("octave_-9");
        CHECK(lo == 0x082FE255F8BE6631ULL);
        CHECK(hi == 0x4E96119E22DEDC81ULL);
    }

    {
        const auto [lo, hi] = md5_seed("minecraft:continentalness");
        CHECK(lo == 0x83886C9D0AE3A662ULL);
        CHECK(hi == 0xAFA638A61B42E8ADULL);
    }

    {
        const auto [lo, hi] = md5_seed("minecraft:erosion");
        CHECK(lo == 0xD02491E6058F6FD8ULL);
        CHECK(hi == 0x4792512C94C17A80ULL);
    }

    {
        const auto [lo, hi] = md5_seed("minecraft:ridge");
        CHECK(lo == 0xEFC8EF4D36102B34ULL);
        CHECK(hi == 0x1BEEEB324A0F24EAULL);
    }

    {
        const auto [lo, hi] = md5_seed("minecraft:offset");
        CHECK(lo == 0x080518CF6AF25384ULL);
        CHECK(hi == 0x3F3DFB40A54FEBD5ULL);
    }

    {
        const auto [lo, hi] = md5_seed("minecraft:temperature");
        CHECK(lo == 0x5C7E6B29735F0D7FULL);
        CHECK(hi == 0xF7D86F1BBC734988ULL);
    }

    {
        const auto [lo, hi] = md5_seed("minecraft:vegetation");
        CHECK(lo == 0x81BB4D22E8DC168EULL);
        CHECK(hi == 0xF1C8B4BEA16303CDULL);
    }

    CHECK(block_seed(0, 0, 0) == 0);
    CHECK(block_seed(1, 0, 0) != block_seed(0, 0, 0));
    CHECK(block_seed(-1, 0, 0) == block_seed(-1, 0, 0));

    {
        xoroshiro128pp random = xoroshiro128pp::from_seed(0);
        const std::array<double, 9> amplitudes{1, 1, 2, 2, 2, 1, 1, 1, 1};
        const normal_noise noise = normal_noise::create(random, -9, amplitudes);
        CHECK_NEAR(noise.max_value(), 7.315068493150684, 1e-12);
        const double value = noise.sample(0.0, 0.0, 0.0);
        CHECK(value >= -noise.max_value() && value <= noise.max_value());
    }

    {
        xoroshiro128pp random = xoroshiro128pp::from_seed(0);
        const std::array<double, 6> amplitudes{1.5, 0, 1, 0, 0, 0};
        const normal_noise noise = normal_noise::create(random, -10, amplitudes);
        CHECK_NEAR(noise.max_value(), 4.444444444444445, 1e-12);
    }

    {
        xoroshiro128pp random = xoroshiro128pp::from_seed(0);
        const std::array<double, 3> amplitudes{1, 1, 1};
        const normal_noise noise = normal_noise::create(random, -6, amplitudes);
        CHECK_NEAR(noise.max_value(), 5.0, 1e-12);
    }

    {
        xoroshiro128pp random = xoroshiro128pp::from_seed(0);
        const std::array<double, 4> amplitudes{1, 1, 0, 1};
        const normal_noise noise = normal_noise::create(random, -6, amplitudes);
        CHECK_NEAR(noise.max_value(), 4.622222222222222, 1e-12);
    }

    {
        const double wrapped = perlin_noise::wrap(33554432.0 * 3.0 + 5.0);
        CHECK_NEAR(wrapped, 5.0, 1e-9);
        CHECK_NEAR(perlin_noise::wrap(1.5), 1.5, 1e-12);
    }

    {
        xoroshiro128pp random = xoroshiro128pp::from_seed(12345);
        const blended_noise noise = blended_noise::create(random, 0.25, 0.125, 80.0, 160.0, 8.0);
        CHECK_NEAR(noise.max_value(), 87.5515, 1e-3);
        const double value = noise.sample(0.0, 64.0, 0.0);
        CHECK(std::isfinite(value));
        CHECK(std::fabs(value) < 1.0);
    }

    {
        xoroshiro128pp random = xoroshiro128pp::from_seed(0);
        random.consume(262);
        CHECK(random.next_long() != 0);
    }
}

}  // namespace batholith::test
