#include "assert.hxx"

#include <batholith/block.hxx>
#include <batholith/chunk.hxx>
#include <batholith/math.hxx>
#include <batholith/surface.hxx>

namespace batholith::test {

void run_chunk() {
    CHECK(blocks_per_chunk == 98304);
    CHECK(lattice_points == 1225);
    CHECK(lattice_x == 5);
    CHECK(lattice_y == 49);
    CHECK(chunk_size_y == 384);
    CHECK(min_y == -64);
    CHECK(sea_level == 63);

    chunk_buffer buffer;
    buffer.set(0, -64, 0, block_id::bedrock);
    buffer.set(15, 319, 15, block_id::stone);
    CHECK(buffer.at(0, -64, 0) == block_id::bedrock);
    CHECK(buffer.at(15, 319, 15) == block_id::stone);
    CHECK(buffer.at(1, 0, 1) == block_id::air);

    CHECK(!is_solid(block_id::air));
    CHECK(!is_solid(block_id::water));
    CHECK(is_solid(block_id::stone));
    CHECK(is_fluid(block_id::lava));

    CHECK(floor_div(-1, 4) == -1);
    CHECK(floor_div(-4, 4) == -1);
    CHECK(floor_div(-5, 4) == -2);
    CHECK(floor_div(7, 4) == 1);
    CHECK(floor_to_int(-0.5) == -1);
    CHECK(floor_to_int(0.5) == 0);
    CHECK(clamp(5, 0, 3) == 3);
    CHECK(clamp(-1, 0, 3) == 0);

    CHECK(surface_material(surface_sample{-64, 0, 0, false}, biome_class::plains) ==
          block_id::bedrock);
    CHECK(surface_material(surface_sample{319, 0, 0, false}, biome_class::plains) ==
          block_id::bedrock);
    CHECK(surface_material(surface_sample{64, 0, 0, false}, biome_class::plains) ==
          block_id::grass_block);
    CHECK(surface_material(surface_sample{64, 0, 2, false}, biome_class::plains) ==
          block_id::dirt);
    CHECK(surface_material(surface_sample{64, 0, 20, false}, biome_class::plains) ==
          block_id::stone);
    CHECK(surface_material(surface_sample{0, 0, 4, false}, biome_class::plains) ==
          block_id::deepslate);
    CHECK(surface_material(surface_sample{64, 0, 0, false}, biome_class::desert) ==
          block_id::sand);
    CHECK(surface_material(surface_sample{64, 0, 0, true}, biome_class::taiga) ==
          block_id::snow_block);
}

}  // namespace batholith::test
