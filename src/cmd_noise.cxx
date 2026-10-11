// Equal-math noise benchmark: Minecraft's continentalness noise, four implementations.
//
// All engines evaluate the same 9-octave Perlin sum (firstOctave -9, amplitudes
// {1,1,2,2,2,1,1,1,1}, both of vanilla's ladders). FastNoise2 uses an explicit additive
// node tree because FractalFBm cannot express MC's non-geometric amplitudes. Phase 1
// checks agreement, phase 2 speed; see docs/benchmark.md.
//
// Usage: batholith noise [--samples N] [--reps N] [--seed N]

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

#include <FastNoise/FastNoise.h>

#include <batholith/noise.hxx>
#include <batholith/rng.hxx>

#include "commands.hxx"

extern "C" void crosscheck_double_perlin_init(std::uint64_t xlo, std::uint64_t xhi,
                                              const double* amplitudes, int omin, int length);
extern "C" double crosscheck_double_perlin_sample(double x, double y, double z);

namespace {

void print_usage() {
    std::fputs("usage: batholith noise [--samples N] [--reps N] [--seed N]\n"
               "\nCompares batholith, cubiomes and FastNoise2 on Minecraft's continentalness\n"
               "noise. Phase 1 checks that the outputs agree, phase 2 compares throughput.\n"
               "  --samples N  target samples per run (default 20000000)\n"
               "  --reps N     timed runs per engine, best kept (default 5)\n"
               "  --seed N     world seed (default 0)\n",
               stderr);
}

constexpr int grid_side = 16;
constexpr int grid_points = grid_side * grid_side;
constexpr double continentalness_amplitudes[] = {1, 1, 2, 2, 2, 1, 1, 1, 1};
constexpr int continentalness_first_octave = -9;
constexpr int continentalness_octaves = 9;

[[nodiscard]] double now_seconds() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

// Minecraft normalises octave i by amplitudes[i] * 2^(count-1-i) / (2^count - 1).
[[nodiscard]] double octave_weight(int octave) {
    const double top = std::pow(2.0, continentalness_octaves - 1 - octave);
    const double divisor = std::pow(2.0, continentalness_octaves) - 1.0;
    return continentalness_amplitudes[octave] * top / divisor;
}

[[nodiscard]] batholith::normal_noise make_batholith(std::uint64_t seed,
                                                     batholith::xoroshiro128pp& octave_source) {
    batholith::xoroshiro128pp random = batholith::xoroshiro128pp::from_seed(seed);
    const batholith::positional_factory factory{random.next_long(), random.next_long()};
    octave_source = factory.from_hash_of("minecraft:continentalness");
    batholith::xoroshiro128pp source = octave_source;
    const std::vector<double> amplitudes(std::begin(continentalness_amplitudes),
                                         std::end(continentalness_amplitudes));
    return batholith::normal_noise::create(source, continentalness_first_octave, amplitudes);
}

// MC's normal_noise is two Perlin ladders: octA at the input, octB at 337/331 x input.
[[nodiscard]] FastNoise::SmartNode<> make_fastnoise2() {
    constexpr double second_ladder_input_scale = 337.0 / 331.0;
    constexpr double continentalness_value_factor = 1.4999999999999998;

    FastNoise::SmartNode<> sum;
    const auto append = [&sum](FastNoise::SmartNode<> node) {
        if (!sum) {
            sum = node;
            return;
        }
        auto add = FastNoise::New<FastNoise::Add>();
        add->SetLHS(sum);
        add->SetRHS(node);
        sum = add;
    };

    for (int ladder = 0; ladder < 2; ++ladder) {
        const double ladder_scale = ladder == 0 ? 1.0 : second_ladder_input_scale;
        for (int octave = 0; octave < continentalness_octaves; ++octave) {
            auto perlin = FastNoise::New<FastNoise::Perlin>();
            // MC evaluates octave k at 2^(firstOctave + k); FN2's scale is 1/frequency.
            const double frequency =
                std::pow(2.0, continentalness_first_octave + octave) / ladder_scale;
            perlin->SetScale(static_cast<float>(1.0 / frequency));
            perlin->SetSeedOffset(ladder * continentalness_octaves + octave);

            auto scaled = FastNoise::New<FastNoise::Multiply>();
            scaled->SetLHS(perlin);
            scaled->SetRHS(static_cast<float>(octave_weight(octave) *
                                             continentalness_value_factor));
            append(scaled);
        }
    }
    return sum;
}

struct position_set {
    std::vector<float> x;
    std::vector<float> z;
};

[[nodiscard]] position_set make_positions() {
    position_set positions;
    positions.x.resize(grid_points);
    positions.z.resize(grid_points);
    for (int index = 0; index < grid_points; ++index) {
        // Spread over several dominant-octave periods; a narrower window is degenerate.
        positions.x[static_cast<std::size_t>(index)] =
            static_cast<float>(index % grid_side) * 64.0F;
        positions.z[static_cast<std::size_t>(index)] =
            static_cast<float>(index / grid_side) * 64.0F;
    }
    return positions;
}

struct deviation {
    double max_abs = 0.0;
    double mean_abs = 0.0;
};

[[nodiscard]] deviation compare(const std::vector<double>& reference,
                                const std::vector<double>& other) {
    deviation result;
    for (std::size_t index = 0; index < reference.size(); ++index) {
        const double error = std::fabs(reference[index] - other[index]);
        result.max_abs = error > result.max_abs ? error : result.max_abs;
        result.mean_abs += error;
    }
    result.mean_abs /= static_cast<double>(reference.size());
    return result;
}

struct statistics {
    double mean = 0.0;
    double standard_deviation = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
};

[[nodiscard]] statistics describe(const std::vector<double>& values) {
    statistics result;
    result.minimum = values.front();
    result.maximum = values.front();
    for (const double value : values) {
        result.mean += value;
        result.minimum = value < result.minimum ? value : result.minimum;
        result.maximum = value > result.maximum ? value : result.maximum;
    }
    result.mean /= static_cast<double>(values.size());
    for (const double value : values) {
        const double delta = value - result.mean;
        result.standard_deviation += delta * delta;
    }
    result.standard_deviation =
        std::sqrt(result.standard_deviation / static_cast<double>(values.size()));
    return result;
}

void report(const char* name, const deviation& measured) {
    std::printf("  %-30s max_abs=%.17g mean_abs=%.17g\n", name, measured.max_abs, measured.mean_abs);
}

void report_stats(const char* name, const std::vector<double>& values) {
    const statistics stats = describe(values);
    std::printf("  %-18s mean=%9.6f stddev=%8.6f range=[%.6f, %.6f]\n", name, stats.mean,
                stats.standard_deviation, stats.minimum, stats.maximum);
}

struct engine_result {
    const char* name;
    double ns_per_sample;
};

}  // namespace

namespace batholith {

int run_noise(int argc, char** argv) {
    long long target_samples = 20'000'000;
    int reps = 5;
    std::uint64_t seed = 0;
    for (int index = 1; index < argc; ++index) {
        const std::string_view flag = argv[index];
        if (flag == "--help" || flag == "-h") {
            print_usage();
            return 0;
        }
    }
    for (int index = 1; index + 1 < argc; index += 2) {
        const std::string_view flag = argv[index];
        if (flag == "--samples") {
            target_samples = std::strtoll(argv[index + 1], nullptr, 10);
        } else if (flag == "--reps") {
            reps = std::atoi(argv[index + 1]);
        } else if (flag == "--seed") {
            seed = std::strtoull(argv[index + 1], nullptr, 10);
        } else {
            std::fprintf(stderr, "unknown flag: %s\n", argv[index]);
            return 2;
        }
    }
    if (target_samples <= 0 || reps <= 0) {
        print_usage();
        return 2;
    }

    batholith::xoroshiro128pp octave_source;
    const batholith::normal_noise batholith_noise = make_batholith(seed, octave_source);
    const FastNoise::SmartNode<> fastnoise2 = make_fastnoise2();
    const position_set positions = make_positions();

    // --- Phase 1: agreement -------------------------------------------------------
    std::vector<double> reference(grid_points);
    for (int index = 0; index < grid_points; ++index) {
        reference[static_cast<std::size_t>(index)] =
            batholith_noise.sample(static_cast<double>(positions.x[static_cast<std::size_t>(index)]),
                                   0.0,
                                   static_cast<double>(positions.z[static_cast<std::size_t>(index)]));
    }

    crosscheck_double_perlin_init(octave_source.lo(), octave_source.hi(),
                                  continentalness_amplitudes, continentalness_first_octave,
                                  continentalness_octaves);
    std::vector<double> cubiomes_values(grid_points);
    for (int index = 0; index < grid_points; ++index) {
        cubiomes_values[static_cast<std::size_t>(index)] = crosscheck_double_perlin_sample(
            static_cast<double>(positions.x[static_cast<std::size_t>(index)]), 0.0,
            static_cast<double>(positions.z[static_cast<std::size_t>(index)]));
    }

    std::vector<double> fastnoise2_scalar(grid_points);
    for (int index = 0; index < grid_points; ++index) {
        fastnoise2_scalar[static_cast<std::size_t>(index)] = static_cast<double>(
            fastnoise2->GenSingle3D(positions.x[static_cast<std::size_t>(index)], 0.0F,
                                    positions.z[static_cast<std::size_t>(index)], 0));
    }

    std::vector<float> fastnoise2_batch(grid_points);
    std::vector<float> zero_y(grid_points, 0.0F);
    fastnoise2->GenPositionArray3D(fastnoise2_batch.data(), grid_points, positions.x.data(),
                                   zero_y.data(), positions.z.data(), 0.0F, 0.0F, 0.0F, 0);

    std::vector<double> fastnoise2_batch_double(grid_points);
    for (int index = 0; index < grid_points; ++index) {
        fastnoise2_batch_double[static_cast<std::size_t>(index)] =
            static_cast<double>(fastnoise2_batch[static_cast<std::size_t>(index)]);
    }

    std::printf("phase1_agreement\n");
    report("batholith_vs_cubiomes", compare(reference, cubiomes_values));
    report("batholith_vs_fastnoise2_scalar", compare(reference, fastnoise2_scalar));
    report("batholith_vs_fastnoise2_batch", compare(reference, fastnoise2_batch_double));
    std::printf("\nphase1_distribution\n");
    report_stats("batholith", reference);
    report_stats("cubiomes", cubiomes_values);
    report_stats("fastnoise2", fastnoise2_scalar);
    std::printf("  fastnoise2 tree: 18 Perlin octaves (2 ladders), MC octave weights\n");

    // --- Phase 2: throughput ------------------------------------------------------
    const long long iterations = (target_samples + grid_points - 1) / grid_points;
    const long long actual_samples = iterations * grid_points;
    std::printf("\nphase2_throughput samples=%lld grid=%d reps=%d\n", actual_samples, grid_points,
                reps);

    std::vector<double> scratch(grid_points);
    const auto run_batholith = [&] {
        for (long long iteration = 0; iteration < iterations; ++iteration) {
            const double offset_x = static_cast<double>(iteration) * 137.0;
            const double offset_z = static_cast<double>(iteration) * 211.0;
            for (int index = 0; index < grid_points; ++index) {
                scratch[static_cast<std::size_t>(index)] = batholith_noise.sample(
                    static_cast<double>(positions.x[static_cast<std::size_t>(index)]) + offset_x, 0.0,
                    static_cast<double>(positions.z[static_cast<std::size_t>(index)]) + offset_z);
            }
        }
    };

    const auto run_cubiomes = [&] {
        for (long long iteration = 0; iteration < iterations; ++iteration) {
            const double offset_x = static_cast<double>(iteration) * 137.0;
            const double offset_z = static_cast<double>(iteration) * 211.0;
            for (int index = 0; index < grid_points; ++index) {
                scratch[static_cast<std::size_t>(index)] = crosscheck_double_perlin_sample(
                    static_cast<double>(positions.x[static_cast<std::size_t>(index)]) + offset_x, 0.0,
                    static_cast<double>(positions.z[static_cast<std::size_t>(index)]) + offset_z);
            }
        }
    };

    const auto run_fastnoise2_scalar = [&] {
        for (long long iteration = 0; iteration < iterations; ++iteration) {
            const float offset_x = static_cast<float>(iteration) * 137.0F;
            const float offset_z = static_cast<float>(iteration) * 211.0F;
            for (int index = 0; index < grid_points; ++index) {
                scratch[static_cast<std::size_t>(index)] = static_cast<double>(
                    fastnoise2->GenSingle3D(positions.x[static_cast<std::size_t>(index)] + offset_x,
                                            0.0F,
                                            positions.z[static_cast<std::size_t>(index)] + offset_z,
                                            0));
            }
        }
    };

    const auto run_fastnoise2_batch = [&] {
        for (long long iteration = 0; iteration < iterations; ++iteration) {
            fastnoise2->GenPositionArray3D(fastnoise2_batch.data(), grid_points, positions.x.data(),
                                           zero_y.data(), positions.z.data(),
                                           static_cast<float>(iteration) * 137.0F, 0.0F,
                                           static_cast<float>(iteration) * 211.0F, 0);
        }
    };

    std::vector<engine_result> results;
    const auto bench = [&](const char* name, const auto& fn) {
        double best = 0.0;
        for (int rep = 0; rep < reps; ++rep) {
            const double start = now_seconds();
            fn();
            const double elapsed = now_seconds() - start;
            best = (rep == 0 || elapsed < best) ? elapsed : best;
        }
        const double ns_per_sample = best * 1e9 / static_cast<double>(actual_samples);
        results.push_back(engine_result{name, ns_per_sample});
        std::printf("  %-18s best=%.4fs  %.3f ns/sample\n", name, best, ns_per_sample);
    };

    bench("batholith", run_batholith);
    bench("cubiomes", run_cubiomes);
    bench("fastnoise2", run_fastnoise2_scalar);
    bench("fastnoise2_batch", run_fastnoise2_batch);

    std::printf("\nphase2_summary\n");
    for (const engine_result& result : results) {
        std::printf("  %-18s %.3f ns/sample  %.2f Msample/s\n", result.name, result.ns_per_sample,
                    1000.0 / result.ns_per_sample);
    }

    // Keep phase 1's values observable so the compiler cannot discard them.
    double sink = 0.0;
    for (const double value : reference) {
        sink += value;
    }
    std::printf("\nchecksum=%.9f\n", sink);
    // A few double ULP; observed worst case ~1.4e-15, so this carries ample margin.
    return compare(reference, cubiomes_values).max_abs < 1e-13 ? 0 : 1;
}

}  // namespace batholith
