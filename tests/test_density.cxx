#include "assert.hxx"

#include <batholith/density.hxx>
#include <batholith/router.hxx>

namespace batholith::test {

void run_density() {
    const world_router router(0);
    CHECK(router.noises().size() == 43);

    density_workspace work;
    const density_graph& graph = router.graph();
    graph.prepare(work, router.noises(), 0, 0);

    const int final_density = router.node(router_output::final_density);
    const double surface = graph.sample(final_density, 0.0, 64.0, 0.0, work, router.noises());
    const double deep = graph.sample(final_density, 0.0, -64.0, 0.0, work, router.noises());
    const double sky = graph.sample(final_density, 0.0, 300.0, 0.0, work, router.noises());

    CHECK(std::isfinite(surface));
    CHECK(std::isfinite(deep));
    CHECK(std::isfinite(sky));
    CHECK(deep > sky);

    const int temperature = router.node(router_output::temperature);
    const double temp = graph.sample(temperature, 0.0, 0.0, 0.0, work, router.noises());
    CHECK(std::isfinite(temp));
    CHECK(std::fabs(temp) < 8.0);

    density_workspace fresh;
    graph.prepare(fresh, router.noises(), 0, 0);
    CHECK_NEAR(graph.sample(final_density, 0.0, 64.0, 0.0, fresh, router.noises()), surface, 0.0);

    // Terrain must actually be solid underground and empty high up, on many columns.
    int solid_columns = 0;
    int empty_columns = 0;
    for (int z = 0; z < 16; ++z) {
        for (int x = 0; x < 16; ++x) {
            const double px = static_cast<double>(x);
            const double pz = static_cast<double>(z);
            if (graph.sample(final_density, px, 0.0, pz, work, router.noises()) > 0.0) {
                ++solid_columns;
            }
            if (graph.sample(final_density, px, 300.0, pz, work, router.noises()) <= 0.0) {
                ++empty_columns;
            }
        }
    }
    CHECK(solid_columns == 256);
    CHECK(empty_columns == 256);
}

}  // namespace batholith::test
