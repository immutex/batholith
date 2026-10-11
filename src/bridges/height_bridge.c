/* Bridge for cubiomes' approximate surface-height generator.
 *
 * Kept in C because cubiomes declares a plain lerp() that collides with std::lerp.
 * Fills a width x height block area at 1:4 horizontal resolution.
 */
#include <cubiomes/generator.h>

static Generator generator;
static SurfaceNoise surface_noise;
static int ready = 0;

int crosscheck_approx_height(float *out, int width, int height, int origin_x, int origin_z) {
    if (!ready) {
        setupGenerator(&generator, MC_1_21_3, 0);
        applySeed(&generator, DIM_OVERWORLD, 0);
        initSurfaceNoise(&surface_noise, DIM_OVERWORLD, 0);
        ready = 1;
    }
    return mapApproxHeight(out, NULL, &generator, &surface_noise, origin_x, origin_z, width, height);
}
