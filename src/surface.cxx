#include <batholith/surface.hxx>

namespace batholith {
namespace {

constexpr int deepslate_top = 8;
constexpr int bedrock_floor = -64;
constexpr int bedrock_roof = 319;
constexpr int bedrock_floor_thickness = 5;
constexpr int dirt_depth = 3;

[[nodiscard]] constexpr bool is_sandy(biome_class biome) noexcept {
    return biome == biome_class::desert || biome == biome_class::beach ||
           biome == biome_class::snowy_beach || biome == biome_class::badlands;
}

[[nodiscard]] constexpr bool is_snowy(biome_class biome) noexcept {
    return biome == biome_class::snowy_beach || biome == biome_class::snowy_mountains ||
           biome == biome_class::taiga;
}

[[nodiscard]] constexpr bool is_stony(biome_class biome) noexcept {
    return biome == biome_class::mountains || biome == biome_class::stony_shore ||
           biome == biome_class::windswept_hills;
}

}  // namespace

block_id surface_material(const surface_sample& sample, biome_class biome) noexcept {
    if (sample.y == bedrock_floor ||
        (sample.y < bedrock_floor + bedrock_floor_thickness &&
         sample.surface_depth + sample.y - bedrock_floor < bedrock_floor_thickness) ||
        sample.y >= bedrock_roof) {
        return block_id::bedrock;
    }
    if (sample.y < deepslate_top) {
        return block_id::deepslate;
    }

    if (is_snowy(biome) && sample.cold_enough_for_snow) {
        return sample.stone_depth_above == 0 ? block_id::snow_block : block_id::dirt;
    }
    if (is_sandy(biome)) {
        if (biome == biome_class::beach || biome == biome_class::snowy_beach) {
            if (sample.stone_depth_above <= dirt_depth + sample.surface_depth) {
                return block_id::sand;
            }
            return sample.stone_depth_above <= 2 * dirt_depth ? block_id::sandstone
                                                              : block_id::stone;
        }
        if (sample.stone_depth_above <= dirt_depth + sample.surface_depth) {
            return biome == biome_class::badlands ? block_id::red_sand : block_id::sand;
        }
        return biome == biome_class::badlands ? block_id::terracotta : block_id::sandstone;
    }
    if (is_stony(biome)) {
        return sample.stone_depth_above == 0 ? block_id::grass_block : block_id::stone;
    }
    if (sample.stone_depth_above == 0) {
        return block_id::grass_block;
    }
    if (sample.stone_depth_above <= dirt_depth + sample.surface_depth) {
        return block_id::dirt;
    }
    return block_id::stone;
}

biome_class classify_biome(double temperature, double vegetation, double erosion, double depth,
                           double continents) noexcept {
    if (depth > 0.45 && continents < -0.11) {
        return biome_class::ocean;
    }
    if (temperature < -0.45) {
        if (vegetation < -0.1) {
            return biome_class::snowy_mountains;
        }
        return vegetation < 0.3 ? biome_class::taiga : biome_class::snowy_beach;
    }
    if (temperature > 0.55 && vegetation < -0.1) {
        return biome_class::desert;
    }
    if (erosion < -0.78) {
        return biome_class::mountains;
    }
    if (erosion > 0.55 && vegetation > 0.1) {
        return biome_class::swamp;
    }
    if (temperature > 0.2 && erosion < -0.35) {
        return biome_class::badlands;
    }
    if (erosion > 0.3 && vegetation > 0.2) {
        return biome_class::plains;
    }
    if (erosion < -0.55) {
        return biome_class::windswept_hills;
    }
    if (erosion > 0.5) {
        return biome_class::beach;
    }
    return erosion < -0.2 ? biome_class::stony_shore : biome_class::plains;
}

}  // namespace batholith
