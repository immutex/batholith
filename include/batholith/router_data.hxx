#pragma once

#include <cstdint>
#include <span>

#include <batholith/density.hxx>

namespace batholith {

struct noise_definition {
    const char* name;
    bool blended;
    int first_octave;
    std::span<const double> amplitudes;
    double xz_scale;
    double y_scale;
    double xz_factor;
    double y_factor;
    double smear;
};

struct router_data {
    std::span<const noise_definition> noises;
    std::span<const density_node> nodes;
    std::span<const spline_point> points;
    std::span<const int> outputs;
};

enum class router_output : std::uint8_t {
    final_density,
    fluid_level_floodedness,
    fluid_level_spread,
    lava,
    temperature,
    vegetation,
    continents,
    erosion,
    depth,
};

[[nodiscard]] const router_data& overworld_router_data() noexcept;

}  // namespace batholith
