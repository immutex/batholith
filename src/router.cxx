#include <batholith/router.hxx>

#include <string>
#include <vector>

#include <batholith/rng.hxx>

namespace batholith {
namespace {

constexpr router_output prepared_outputs[] = {
    router_output::final_density,
    router_output::fluid_level_floodedness,
    router_output::fluid_level_spread,
    router_output::lava,
    router_output::temperature,
    router_output::vegetation,
    router_output::continents,
    router_output::erosion,
    router_output::depth,
};

[[nodiscard]] noise_registry build_noises(const router_data& data, std::uint64_t seed) {
    xoroshiro128pp random = xoroshiro128pp::from_seed(seed);
    const positional_factory factory{random.next_long(), random.next_long()};

    noise_registry registry;
    for (const noise_definition& definition : data.noises) {
        // Vanilla derives each noise seed from the MD5 of "minecraft:<name>".
        const std::string name = "minecraft:" + std::string(definition.name);
        xoroshiro128pp source = factory.from_hash_of(name);
        if (definition.blended) {
            registry.add(blended_noise::create(source, definition.xz_scale, definition.y_scale,
                                               definition.xz_factor, definition.y_factor,
                                               definition.smear));
        } else {
            registry.add(
                normal_noise::create(source, definition.first_octave, definition.amplitudes));
        }
    }
    return registry;
}

}  // namespace

std::vector<int> world_router::prepared_node_indices() const {
    std::vector<int> indices;
    indices.reserve(std::size(prepared_outputs));
    for (const router_output which : prepared_outputs) {
        indices.push_back(node(which));
    }
    return indices;
}

world_router::world_router(std::uint64_t seed)
    : data_(overworld_router_data()),
      noises_(build_noises(data_, seed)),
      graph_(std::vector<density_node>(data_.nodes.begin(), data_.nodes.end()),
             std::vector<spline_point>(data_.points.begin(), data_.points.end()),
             prepared_node_indices()) {}

}  // namespace batholith
