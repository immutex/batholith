#pragma once

#include <cstdint>

#include <batholith/block.hxx>
#include <batholith/density.hxx>
#include <batholith/router.hxx>

namespace batholith {

struct aquifer_sample {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    int min_surface_level = 0;
};

// Decides whether air below sea level becomes water, lava, or stays dry. Follows the
// vanilla floodedness model; the cross-chunk "nearby surface" probe is not implemented.
class aquifer {
public:
    explicit aquifer(const world_router& router) noexcept;

    [[nodiscard]] block_id fluid(const aquifer_sample& sample, density_workspace& work) const
        noexcept;

private:
    [[nodiscard]] double floodedness(const aquifer_sample& sample, density_workspace& work) const
        noexcept;

    const world_router* router_;
    int floodedness_node_;
    int spread_node_;
    int lava_node_;
};

}  // namespace batholith
