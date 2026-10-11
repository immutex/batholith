#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <batholith/rng.hxx>

namespace batholith {

class improved_noise {
public:
    // Default-constructible only so perlin_noise can size its octave table; an
    // octave that is never assigned is never sampled.
    improved_noise() = default;

    static improved_noise from_random(xoroshiro128pp& random) noexcept;

    [[nodiscard]] double sample(double x, double y, double z, double y_scale,
                                double y_max) const noexcept;

private:
    [[nodiscard]] int permute(int index) const noexcept {
        return static_cast<int>(permutation_[static_cast<std::size_t>(index) & 255]);
    }

    std::array<std::uint8_t, 256> permutation_{};
    double xo_ = 0.0;
    double yo_ = 0.0;
    double zo_ = 0.0;
};

class perlin_noise {
public:
    static perlin_noise positional(xoroshiro128pp& random, int first_octave,
                                   std::span<const double> amplitudes) noexcept;
    static perlin_noise legacy(xoroshiro128pp& random, int first_octave,
                               std::span<const double> amplitudes) noexcept;

    [[nodiscard]] double sample(double x, double y, double z) const noexcept;
    [[nodiscard]] double max_value() const noexcept { return max_value_; }
    [[nodiscard]] double max_broken_value(double edge) const noexcept;
    [[nodiscard]] const improved_noise* octave_noise(int index) const noexcept;

    [[nodiscard]] static double wrap(double value) noexcept;

private:
    [[nodiscard]] std::vector<const improved_noise*> level_pointers() const;

    [[nodiscard]] static double edge_value(std::span<const improved_noise* const> levels,
                                           std::span<const double> amplitudes,
                                           double lowest_freq_value_factor, double edge) noexcept;

    std::vector<improved_noise> levels_;
    std::vector<bool> present_;
    std::vector<double> amplitudes_;
    int first_octave_ = 0;
    double lowest_freq_input_factor_ = 1.0;
    double lowest_freq_value_factor_ = 1.0;
    double max_value_ = 0.0;
};

class normal_noise {
public:
    static normal_noise create(xoroshiro128pp& random, int first_octave,
                               std::span<const double> amplitudes) noexcept;

    [[nodiscard]] double sample(double x, double y, double z) const noexcept;

    // Exposed because the normalisation factor is otherwise unobservable, and it is
    // the only thing standing between correct and plausible-looking noise.
    [[nodiscard]] double max_value() const noexcept { return max_value_; }

private:
    perlin_noise first_;
    perlin_noise second_;
    double value_factor_ = 1.0;
    double max_value_ = 0.0;
};

class blended_noise {
public:
    static blended_noise create(xoroshiro128pp& random, double xz_scale, double y_scale,
                                double xz_factor, double y_factor,
                                double smear_scale_multiplier) noexcept;

    [[nodiscard]] double sample(double x, double y, double z) const noexcept;
    [[nodiscard]] double max_value() const noexcept { return max_value_; }

private:
    perlin_noise min_limit_;
    perlin_noise max_limit_;
    perlin_noise main_;
    double xz_multiplier_ = 1.0;
    double y_multiplier_ = 1.0;
    double xz_factor_ = 1.0;
    double y_factor_ = 1.0;
    double smear_scale_multiplier_ = 1.0;
    double max_value_ = 1.0;
};

}  // namespace batholith
