#include <batholith/generator.hxx>

#include <chrono>
#include <vector>

namespace batholith {
namespace {

constexpr double snow_temperature = -0.45;

// Scratch is per-thread so the chunk loop never allocates after the first chunk.
thread_local density_workspace scratch;

[[nodiscard]] generation_stats make_stats(std::size_t chunks, double seconds) noexcept {
    return generation_stats{chunks, chunks * blocks_per_chunk, seconds};
}

}  // namespace

generator::generator(std::uint64_t seed) : router_(seed), aquifer_(router_) {}

void generator::generate_into(const chunk_pos& pos, chunk_buffer& out,
                              density_workspace& work) const noexcept {
    const int base_x = world_min_x(pos);
    const int base_z = world_min_z(pos);
    const density_graph& graph = router_.graph();
    const noise_registry& noises = router_.noises();

    const int final_density = router_.node(router_output::final_density);
    const int temperature = router_.node(router_output::temperature);
    const int vegetation = router_.node(router_output::vegetation);
    const int erosion = router_.node(router_output::erosion);
    const int depth = router_.node(router_output::depth);
    const int continents = router_.node(router_output::continents);

    graph.prepare(work, noises, base_x, base_z);

    for (int z = 0; z < chunk_size_z; ++z) {
        for (int x = 0; x < chunk_size_x; ++x) {
            const double wx = static_cast<double>(base_x + x);
            const double wz = static_cast<double>(base_z + z);

            const double temp = graph.sample(temperature, wx, 0.0, wz, work, noises);
            const biome_class biome =
                classify_biome(temp, graph.sample(vegetation, wx, 0.0, wz, work, noises),
                               graph.sample(erosion, wx, 0.0, wz, work, noises),
                               graph.sample(depth, wx, 0.0, wz, work, noises),
                               graph.sample(continents, wx, 0.0, wz, work, noises));

            int stone_depth_above = 0;
            int surface_level = min_y;
            bool seen_solid = false;
            for (int y = chunk_size_y - 1; y >= 0; --y) {
                const int wy = min_y + y;
                const double density =
                    graph.sample(final_density, wx, static_cast<double>(wy), wz, work, noises);
                const bool solid = density > 0.0;
                if (solid && !seen_solid) {
                    seen_solid = true;
                    surface_level = wy;
                }
                if (solid) {
                    out.set(x, wy, z,
                            surface_material(surface_sample{wy, 0, stone_depth_above,
                                                            temp < snow_temperature},
                                             biome));
                    ++stone_depth_above;
                } else if (wy <= sea_level) {
                    out.set(x, wy, z,
                            aquifer_.fluid(aquifer_sample{wx, static_cast<double>(wy), wz,
                                                          surface_level},
                                           work));
                }
            }
        }
    }
}

void generator::generate(const chunk_pos& pos, chunk_buffer& out) const {
    generate_into(pos, out, scratch);
}

generation_stats generator::generate_region(int origin_x, int origin_z, int width, int depth,
                                            thread_pool& pool) const {
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(depth);
    std::vector<chunk_buffer> buffers(count);
    const auto start = std::chrono::steady_clock::now();
    pool.parallel_for(count, [&](std::size_t index) {
        const int cx = origin_x + static_cast<int>(index % static_cast<std::size_t>(width));
        const int cz = origin_z + static_cast<int>(index / static_cast<std::size_t>(width));
        generate_into(chunk_pos{cx, cz}, buffers[index], scratch);
    });
    const auto stop = std::chrono::steady_clock::now();
    return make_stats(count, std::chrono::duration<double>(stop - start).count());
}

generation_stats generator::generate_region(int origin_x, int origin_z, int width,
                                            int depth) const {
    thread_pool pool(thread_pool::hardware_threads());
    return generate_region(origin_x, origin_z, width, depth, pool);
}

}  // namespace batholith
