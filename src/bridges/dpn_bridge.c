/* Bridge for cubiomes' DoublePerlinNoise.
 *
 * Kept in C because cubiomes declares a plain lerp() that collides with std::lerp.
 * Initialisation happens once and the built noise is cached: xDoublePerlinInit shuffles
 * 256-entry permutation tables per octave, so re-initialising per sample would measure
 * table construction rather than noise evaluation.
 */
#include <cubiomes/noise.h>

#define MAX_OCTAVES 64

static DoublePerlinNoise cached;
static PerlinNoise octave_storage[MAX_OCTAVES];
static int cached_valid = 0;

void crosscheck_double_perlin_init(uint64_t xlo, uint64_t xhi, const double *amplitudes,
                                   int omin, int length) {
    Xoroshiro xr;
    xr.lo = xlo;
    xr.hi = xhi;
    xDoublePerlinInit(&cached, &xr, octave_storage, amplitudes, omin, length, -1);
    cached_valid = 1;
}

double crosscheck_double_perlin_sample(double x, double y, double z) {
    return cached_valid ? sampleDoublePerlin(&cached, x, y, z) : 0.0;
}

/* One-shot convenience used by the differential tests. */
double crosscheck_double_perlin(uint64_t xlo, uint64_t xhi, const double *amplitudes, int omin,
                                int length, double x, double y, double z) {
    crosscheck_double_perlin_init(xlo, xhi, amplitudes, omin, length);
    return crosscheck_double_perlin_sample(x, y, z);
}
