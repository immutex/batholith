#pragma once

#include <cstdint>

#include <batholith/block.hxx>

namespace batholith {

enum class biome_class : std::uint8_t {
    plains,
    ocean,
    desert,
    beach,
    snowy_beach,
    mountains,
    snowy_mountains,
    taiga,
    swamp,
    badlands,
    stony_shore,
    windswept_hills,
};

struct surface_sample {
    int y = 0;
    int surface_depth = 0;
    int stone_depth_above = 0;
    bool cold_enough_for_snow = false;
};

// Compiled-in approximation of the overworld surface rule tree, which turns default
// stone into grass, dirt, sand, deepslate and bedrock.
[[nodiscard]] block_id surface_material(const surface_sample& sample, biome_class biome) noexcept;

[[nodiscard]] biome_class classify_biome(double temperature, double vegetation, double erosion,
                                         double depth, double continents) noexcept;

}  // namespace batholith
