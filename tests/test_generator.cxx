#include "assert.hxx"

#include <algorithm>
#include <vector>

#include <batholith/generator.hxx>

namespace batholith::test {

void run_generator() {
    const generator gen(0);
    chunk_buffer buffer;
    gen.generate(chunk_pos{0, 0}, buffer);

    int air = 0;
    int solid = 0;
    int fluid = 0;
    for (const block_id id : buffer.blocks()) {
        if (id == block_id::air) {
            ++air;
        } else if (is_fluid(id)) {
            ++fluid;
        } else {
            ++solid;
        }
    }

    CHECK(air + solid + fluid == static_cast<int>(blocks_per_chunk));
    CHECK(solid > 0);
    CHECK(air > 0);
    CHECK(buffer.at(0, min_y, 0) != block_id::air);

    const chunk_buffer again = [&] {
        chunk_buffer other;
        gen.generate(chunk_pos{0, 0}, other);
        return other;
    }();
    CHECK(again.blocks() == buffer.blocks());

    const generator same_seed(0);
    chunk_buffer identical;
    same_seed.generate(chunk_pos{0, 0}, identical);
    CHECK(identical.blocks() == buffer.blocks());

    const generator other_seed(1);
    chunk_buffer different;
    other_seed.generate(chunk_pos{0, 0}, different);
    CHECK(different.blocks() != buffer.blocks());

    thread_pool pool(2);
    const generation_stats stats = gen.generate_region(0, 0, 2, 2, pool);
    CHECK(stats.chunks == 4);
    CHECK(stats.blocks == 4 * blocks_per_chunk);
    CHECK(stats.seconds > 0.0);
    CHECK(stats.blocks_per_second() > 0.0);

    thread_pool single(1);
    thread_pool wide(4);
    std::vector<chunk_buffer> serial(9);
    std::vector<chunk_buffer> parallel(9);
    single.parallel_for(9, [&](std::size_t index) {
        gen.generate(chunk_pos{static_cast<int>(index % 3), static_cast<int>(index / 3)},
                     serial[index]);
    });
    wide.parallel_for(9, [&](std::size_t index) {
        gen.generate(chunk_pos{static_cast<int>(index % 3), static_cast<int>(index / 3)},
                     parallel[index]);
    });
    bool matches = true;
    for (std::size_t index = 0; index < serial.size(); ++index) {
        matches = matches && serial[index].blocks() == parallel[index].blocks();
    }
    CHECK(matches);

    // Regression: a lattice that ignores the chunk origin clamps every chunk to one
    // corner and flattens the world to a plane.
    const auto surface_profile = [](const chunk_buffer& chunk) {
        std::vector<int> heights;
        heights.reserve(chunk_size_x * chunk_size_z);
        for (int z = 0; z < chunk_size_z; ++z) {
            for (int x = 0; x < chunk_size_x; ++x) {
                int height = min_y - 1;
                for (int y = min_y + chunk_size_y - 1; y >= min_y; --y) {
                    if (is_solid(chunk.at(x, y, z))) {
                        height = y;
                        break;
                    }
                }
                heights.push_back(height);
            }
        }
        return heights;
    };

    const auto height_spread = [](const std::vector<int>& heights) {
        const auto [low, high] = std::minmax_element(heights.begin(), heights.end());
        return *high - *low;
    };

    const chunk_pos spread_origins[] = {
        chunk_pos{0, 0}, chunk_pos{64, 64}, chunk_pos{-40, 25}, chunk_pos{200, -150}};

    int lowest_surface = min_y + chunk_size_y;
    int highest_surface = min_y;
    std::vector<std::vector<int>> profiles;
    for (const chunk_pos& origin : spread_origins) {
        chunk_buffer chunk;
        gen.generate(origin, chunk);
        profiles.push_back(surface_profile(chunk));
        for (const int height : profiles.back()) {
            lowest_surface = height < lowest_surface ? height : lowest_surface;
            highest_surface = height > highest_surface ? height : highest_surface;
        }
    }

    CHECK(highest_surface - lowest_surface > 3);
    // The failure mode is a chunk that is flat in x and z: a lattice that ignores the
    // chunk origin collapses every column in a chunk to one corner value. Terrain here is
    // gentle, so the bar is one block of relief, not a dramatic profile.
    for (const std::vector<int>& profile : profiles) {
        CHECK(height_spread(profile) >= 1);
    }
}

}  // namespace batholith::test
