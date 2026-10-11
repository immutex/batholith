#include <batholith/block.hxx>

namespace batholith {

const char* to_string(block_id id) noexcept {
    switch (id) {
        case block_id::air: return "air";
        case block_id::stone: return "stone";
        case block_id::deepslate: return "deepslate";
        case block_id::bedrock: return "bedrock";
        case block_id::dirt: return "dirt";
        case block_id::coarse_dirt: return "coarse_dirt";
        case block_id::rooted_dirt: return "rooted_dirt";
        case block_id::grass_block: return "grass_block";
        case block_id::podzol: return "podzol";
        case block_id::mycelium: return "mycelium";
        case block_id::sand: return "sand";
        case block_id::red_sand: return "red_sand";
        case block_id::sandstone: return "sandstone";
        case block_id::red_sandstone: return "red_sandstone";
        case block_id::gravel: return "gravel";
        case block_id::clay: return "clay";
        case block_id::water: return "water";
        case block_id::lava: return "lava";
        case block_id::snow_block: return "snow_block";
        case block_id::powder_snow: return "powder_snow";
        case block_id::ice: return "ice";
        case block_id::packed_ice: return "packed_ice";
        case block_id::blue_ice: return "blue_ice";
        case block_id::terracotta: return "terracotta";
        case block_id::orange_terracotta: return "orange_terracotta";
        case block_id::yellow_terracotta: return "yellow_terracotta";
        case block_id::brown_terracotta: return "brown_terracotta";
        case block_id::white_terracotta: return "white_terracotta";
        case block_id::light_gray_terracotta: return "light_gray_terracotta";
        case block_id::calcite: return "calcite";
        case block_id::tuff: return "tuff";
        case block_id::basalt: return "basalt";
        case block_id::blackstone: return "blackstone";
        case block_id::soul_soil: return "soul_soil";
        case block_id::soul_sand: return "soul_sand";
        case block_id::magma_block: return "magma_block";
        case block_id::netherrack: return "netherrack";
        case block_id::moss_block: return "moss_block";
        case block_id::muddy_mangrove_roots: return "muddy_mangrove_roots";
        case block_id::mud: return "mud";
        case block_id::andesite: return "andesite";
        case block_id::granite: return "granite";
        case block_id::diorite: return "diorite";
    }
    return "unknown";
}

}  // namespace batholith
