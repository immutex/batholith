#pragma once

#include <cstdint>

namespace batholith {

enum class block_id : std::uint16_t {
    air = 0,
    stone,
    deepslate,
    bedrock,
    dirt,
    coarse_dirt,
    rooted_dirt,
    grass_block,
    podzol,
    mycelium,
    sand,
    red_sand,
    sandstone,
    red_sandstone,
    gravel,
    clay,
    water,
    lava,
    snow_block,
    powder_snow,
    ice,
    packed_ice,
    blue_ice,
    terracotta,
    orange_terracotta,
    yellow_terracotta,
    brown_terracotta,
    white_terracotta,
    light_gray_terracotta,
    calcite,
    tuff,
    basalt,
    blackstone,
    soul_soil,
    soul_sand,
    magma_block,
    netherrack,
    moss_block,
    muddy_mangrove_roots,
    mud,
    andesite,
    granite,
    diorite,
};

[[nodiscard]] constexpr bool is_solid(block_id id) noexcept {
    return id != block_id::air && id != block_id::water && id != block_id::lava;
}

[[nodiscard]] constexpr bool is_fluid(block_id id) noexcept {
    return id == block_id::water || id == block_id::lava;
}

}  // namespace batholith
