#include <batholith/aquifer.hxx>

#include <cmath>

#include <batholith/chunk.hxx>

namespace batholith {
namespace {

constexpr double lava_threshold = 0.3;
constexpr double min_disabled = 0.8;
constexpr double max_empty = 0.4;
constexpr int lava_level = -54;
constexpr int disabled_band = 12;

}  // namespace

aquifer::aquifer(const world_router& router) noexcept
    : router_(&router),
      floodedness_node_(router.node(router_output::fluid_level_floodedness)),
      spread_node_(router.node(router_output::fluid_level_spread)),
      lava_node_(router.node(router_output::lava)) {}

double aquifer::floodedness(const aquifer_sample& sample, density_workspace& work) const noexcept {
    return router_->graph().sample(floodedness_node_, sample.x, sample.y, sample.z, work,
                                   router_->noises());
}

block_id aquifer::fluid(const aquifer_sample& sample, density_workspace& work) const noexcept {
    if (sample.y < static_cast<double>(lava_level)) {
        const double lava = router_->graph().sample(lava_node_, sample.x, sample.y, sample.z, work,
                                                    router_->noises());
        return std::fabs(lava) <= lava_threshold ? block_id::water : block_id::lava;
    }

    const double surface = static_cast<double>(sample.min_surface_level);
    if (sample.y >= surface + disabled_band || sample.y <= surface - disabled_band) {
        return block_id::water;
    }

    const double value = floodedness(sample, work);
    if (value > min_disabled) {
        return block_id::water;
    }
    return value <= max_empty ? block_id::air : block_id::water;
}

}  // namespace batholith
