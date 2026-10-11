#include <batholith/noise.hxx>

#include <cmath>
#include <cstddef>
#include <string>

#include <batholith/math.hxx>

namespace batholith {
namespace {

// The 16-entry table is SimplexNoise.GRADIENT; entries 12..15 repeat earlier rows and
// are kept verbatim because the gradient is selected by (hash & 15).
constexpr int gradients[16][3] = {
    {1, 1, 0},   {-1, 1, 0},  {1, -1, 0},  {-1, -1, 0}, {1, 0, 1},   {-1, 0, 1},
    {1, 0, -1},  {-1, 0, -1}, {0, 1, 1},   {0, -1, 1},  {0, 1, -1},  {0, -1, -1},
    {1, 1, 0},   {0, -1, 1},  {-1, 1, 0},  {0, -1, -1}};

constexpr double wrap_modulus = 33554432.0;
// Vanilla declares this as a float literal, so it widens to 1.0000000116860974e-07,
// not 1e-7. The distinction shifts results at cell boundaries.
constexpr double shift_up_epsilon = static_cast<double>(1.0e-7F);
constexpr double input_factor = 1.0181268882175227;
constexpr double noise_scale = 684.412;

[[nodiscard]] constexpr double smoothstep(double t) noexcept {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

[[nodiscard]] inline double lerp3(double tx, double ty, double tz, const double* v) noexcept {
    return lerp(tz, lerp(ty, lerp(tx, v[0], v[1]), lerp(tx, v[2], v[3])),
                lerp(ty, lerp(tx, v[4], v[5]), lerp(tx, v[6], v[7])));
}

[[nodiscard]] inline double grad_dot(int hash, double x, double y, double z) noexcept {
    const int* g = gradients[hash & 15];
    return static_cast<double>(g[0]) * x + static_cast<double>(g[1]) * y +
           static_cast<double>(g[2]) * z;
}

inline void skip_octave(xoroshiro128pp& random) noexcept {
    random.consume(262);
}

}  // namespace

improved_noise improved_noise::from_random(xoroshiro128pp& random) noexcept {
    improved_noise result;
    result.xo_ = random.next_double() * 256.0;
    result.yo_ = random.next_double() * 256.0;
    result.zo_ = random.next_double() * 256.0;
    for (int index = 0; index < 256; ++index) {
        result.permutation_[static_cast<std::size_t>(index)] = static_cast<std::uint8_t>(index);
    }
    for (int index = 0; index < 256; ++index) {
        const int offset = random.next_int(256 - index);
        const std::uint8_t swap = result.permutation_[static_cast<std::size_t>(index)];
        result.permutation_[static_cast<std::size_t>(index)] =
            result.permutation_[static_cast<std::size_t>(index + offset)];
        result.permutation_[static_cast<std::size_t>(index + offset)] = swap;
    }
    return result;
}

double improved_noise::sample(double x, double y, double z, double y_scale,
                              double y_max) const noexcept {
    const double px = x + xo_;
    const double py = y + yo_;
    const double pz = z + zo_;
    const int xi = floor_to_int(px);
    const int yi = floor_to_int(py);
    const int zi = floor_to_int(pz);
    const double dx = px - static_cast<double>(xi);
    const double dy = py - static_cast<double>(yi);
    const double dz = pz - static_cast<double>(zi);

    double y_shift = 0.0;
    if (y_scale != 0.0) {
        const double clamped = (y_max >= 0.0 && y_max < dy) ? y_max : dy;
        y_shift = std::floor(clamped / y_scale + shift_up_epsilon) * y_scale;
    }

    const int a = permute(xi);
    const int b = permute(xi + 1);
    const int c = permute(a + yi);
    const int d = permute(a + yi + 1);
    const int e = permute(b + yi);
    const int f = permute(b + yi + 1);

    const double sy = dy - y_shift;
    double v[8];
    v[0] = grad_dot(permute(c + zi), dx, sy, dz);
    v[1] = grad_dot(permute(e + zi), dx - 1.0, sy, dz);
    v[2] = grad_dot(permute(d + zi), dx, sy - 1.0, dz);
    v[3] = grad_dot(permute(f + zi), dx - 1.0, sy - 1.0, dz);
    v[4] = grad_dot(permute(c + zi + 1), dx, sy, dz - 1.0);
    v[5] = grad_dot(permute(e + zi + 1), dx - 1.0, sy, dz - 1.0);
    v[6] = grad_dot(permute(d + zi + 1), dx, sy - 1.0, dz - 1.0);
    v[7] = grad_dot(permute(f + zi + 1), dx - 1.0, sy - 1.0, dz - 1.0);

    return lerp3(smoothstep(dx), smoothstep(dy), smoothstep(dz), v);
}

double perlin_noise::wrap(double value) noexcept {
    return value - std::floor(value / wrap_modulus + 0.5) * wrap_modulus;
}

double perlin_noise::edge_value(std::span<const improved_noise* const> levels,
                                std::span<const double> amplitudes,
                                double lowest_freq_value_factor, double edge) noexcept {
    double sum = 0.0;
    double factor = lowest_freq_value_factor;
    for (std::size_t index = 0; index < levels.size(); ++index) {
        if (levels[index] != nullptr) {
            sum += amplitudes[index] * edge * factor;
        }
        factor /= 2.0;
    }
    return sum;
}

perlin_noise perlin_noise::positional(xoroshiro128pp& random, int first_octave,
                                      std::span<const double> amplitudes) noexcept {
    perlin_noise result;
    result.first_octave_ = first_octave;
    result.amplitudes_.assign(amplitudes.begin(), amplitudes.end());
    result.levels_.resize(amplitudes.size());
    result.present_.assign(amplitudes.size(), false);

    const positional_factory factory{random.next_long(), random.next_long()};
    for (std::size_t index = 0; index < amplitudes.size(); ++index) {
        if (amplitudes[index] == 0.0) {
            continue;
        }
        const std::string name =
            "octave_" + std::to_string(first_octave + static_cast<int>(index));
        xoroshiro128pp source = factory.from_hash_of(name);
        result.levels_[index] = improved_noise::from_random(source);
        result.present_[index] = true;
    }

    const int count = static_cast<int>(amplitudes.size());
    const int base_octave = -first_octave;
    result.lowest_freq_input_factor_ = std::pow(2.0, static_cast<double>(-base_octave));
    result.lowest_freq_value_factor_ =
        std::pow(2.0, static_cast<double>(count - 1)) /
        (std::pow(2.0, static_cast<double>(count)) - 1.0);
    result.max_value_ = result.edge_value(result.level_pointers(), result.amplitudes_,
                                          result.lowest_freq_value_factor_, 2.0);
    return result;
}

perlin_noise perlin_noise::legacy(xoroshiro128pp& random, int first_octave,
                                  std::span<const double> amplitudes) noexcept {
    perlin_noise result;
    result.first_octave_ = first_octave;
    result.amplitudes_.assign(amplitudes.begin(), amplitudes.end());
    result.levels_.resize(amplitudes.size());
    result.present_.assign(amplitudes.size(), false);

    const int start = -first_octave;
    const int count = static_cast<int>(amplitudes.size());
    if (start >= 0 && start < count && amplitudes[static_cast<std::size_t>(start)] != 0.0) {
        result.levels_[static_cast<std::size_t>(start)] = improved_noise::from_random(random);
        result.present_[static_cast<std::size_t>(start)] = true;
    } else {
        skip_octave(random);
    }
    for (int index = start - 1; index >= 0; --index) {
        if (index < count && amplitudes[static_cast<std::size_t>(index)] != 0.0) {
            result.levels_[static_cast<std::size_t>(index)] = improved_noise::from_random(random);
            result.present_[static_cast<std::size_t>(index)] = true;
        } else {
            skip_octave(random);
        }
    }

    result.lowest_freq_input_factor_ = std::pow(2.0, static_cast<double>(-start));
    result.lowest_freq_value_factor_ =
        std::pow(2.0, static_cast<double>(count - 1)) /
        (std::pow(2.0, static_cast<double>(count)) - 1.0);
    result.max_value_ = result.edge_value(result.level_pointers(), result.amplitudes_,
                                          result.lowest_freq_value_factor_, 2.0);
    return result;
}

std::vector<const improved_noise*> perlin_noise::level_pointers() const {
    std::vector<const improved_noise*> pointers(levels_.size(), nullptr);
    for (std::size_t index = 0; index < levels_.size(); ++index) {
        pointers[index] = present_[index] ? &levels_[index] : nullptr;
    }
    return pointers;
}

double perlin_noise::max_broken_value(double edge) const noexcept {
    const std::vector<const improved_noise*> pointers = level_pointers();
    return edge_value(pointers, amplitudes_, lowest_freq_value_factor_, edge + 2.0);
}

const improved_noise* perlin_noise::octave_noise(int index) const noexcept {
    const int target = static_cast<int>(levels_.size()) - 1 - index;
    if (target < 0 || target >= static_cast<int>(levels_.size()) || !present_[static_cast<std::size_t>(target)]) {
        return nullptr;
    }
    return &levels_[static_cast<std::size_t>(target)];
}

double perlin_noise::sample(double x, double y, double z) const noexcept {
    double sum = 0.0;
    double frequency = lowest_freq_input_factor_;
    double factor = lowest_freq_value_factor_;
    for (std::size_t index = 0; index < levels_.size(); ++index) {
        if (present_[index]) {
            const double value = levels_[index].sample(wrap(x * frequency), wrap(y * frequency),
                                                       wrap(z * frequency), 0.0, 0.0);
            sum += amplitudes_[index] * value * factor;
        }
        frequency *= 2.0;
        factor /= 2.0;
    }
    return sum;
}

double normal_noise::sample(double x, double y, double z) const noexcept {
    const double fx = x * input_factor;
    const double fy = y * input_factor;
    const double fz = z * input_factor;
    return (first_.sample(x, y, z) + second_.sample(fx, fy, fz)) * value_factor_;
}

normal_noise normal_noise::create(xoroshiro128pp& random, int first_octave,
                                  std::span<const double> amplitudes) noexcept {
    normal_noise result;
    result.first_ = perlin_noise::positional(random, first_octave, amplitudes);
    result.second_ = perlin_noise::positional(random, first_octave, amplitudes);

    int low = 1 << 30;
    int high = -1;
    for (std::size_t index = 0; index < amplitudes.size(); ++index) {
        if (amplitudes[index] != 0.0) {
            const int position = static_cast<int>(index);
            low = position < low ? position : low;
            high = position > high ? position : high;
        }
    }
    const double span = static_cast<double>(high - low);
    const double deviation = 0.1 * (1.0 + 1.0 / (span + 1.0));
    result.value_factor_ = 0.16666666666666666 / deviation;
    result.max_value_ =
        (result.first_.max_value() + result.second_.max_value()) * result.value_factor_;
    return result;
}

blended_noise blended_noise::create(xoroshiro128pp& random, double xz_scale, double y_scale,
                                    double xz_factor, double y_factor,
                                    double smear_scale_multiplier) noexcept {
    blended_noise result;
    const std::vector<double> wide(16, 1.0);
    const std::vector<double> narrow(8, 1.0);

    result.min_limit_ = perlin_noise::legacy(random, -15, wide);
    result.max_limit_ = perlin_noise::legacy(random, -15, wide);
    result.main_ = perlin_noise::legacy(random, -7, narrow);

    result.xz_multiplier_ = noise_scale * xz_scale;
    result.y_multiplier_ = noise_scale * y_scale;
    result.xz_factor_ = xz_factor;
    result.y_factor_ = y_factor;
    result.smear_scale_multiplier_ = smear_scale_multiplier;
    result.max_value_ = result.min_limit_.max_broken_value(result.y_multiplier_);
    return result;
}

double blended_noise::sample(double x, double y, double z) const noexcept {
    const double bx = x * xz_multiplier_;
    const double by = y * y_multiplier_;
    const double bz = z * xz_multiplier_;
    const double gx = bx / xz_factor_;
    const double hy = by / y_factor_;
    const double iz = bz / xz_factor_;
    const double smear = y_multiplier_ * smear_scale_multiplier_;
    const double smear_y = smear / y_factor_;

    double main = 0.0;
    double octave = 1.0;
    for (int index = 0; index < 8; ++index) {
        if (const improved_noise* noise = main_.octave_noise(index)) {
            main += noise->sample(perlin_noise::wrap(gx * octave), perlin_noise::wrap(hy * octave),
                                  perlin_noise::wrap(iz * octave), smear_y * octave,
                                  hy * octave) /
                    octave;
        }
        octave /= 2.0;
    }

    const double blend = (main / 10.0 + 1.0) / 2.0;
    const bool skip_min = blend >= 1.0;
    const bool skip_max = blend <= 0.0;

    double low = 0.0;
    double high = 0.0;
    octave = 1.0;
    for (int index = 0; index < 16; ++index) {
        const double sx = perlin_noise::wrap(bx * octave);
        const double sy = perlin_noise::wrap(by * octave);
        const double sz = perlin_noise::wrap(bz * octave);
        const double y_amplitude = smear * octave;
        if (!skip_min) {
            if (const improved_noise* noise = min_limit_.octave_noise(index)) {
                low += noise->sample(sx, sy, sz, y_amplitude, by * octave) / octave;
            }
        }
        if (!skip_max) {
            if (const improved_noise* noise = max_limit_.octave_noise(index)) {
                high += noise->sample(sx, sy, sz, y_amplitude, by * octave) / octave;
            }
        }
        octave /= 2.0;
    }

    return clamped_lerp(low / 512.0, high / 512.0, blend) / 128.0;
}

}  // namespace batholith
