// Terrain benchmark: full chunk generation for batholith, with cubiomes' height map
// measured alongside for context.
//
// Comparability limit: cubiomes has no overworld surface height -- for MC >= 1.18
// mapApproxHeight() returns a biome depth parameter, not a block height, so the two are NOT
// compared. The comparable layer is the noise, in `batholith noise`.
//
// Usage: batholith terrain [--side N] [--reps N] [--threads N] [--seed N]

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

#include <batholith/generator.hxx>

#include "commands.hxx"

extern "C" int crosscheck_approx_height(float* out, int width, int height, int origin_x,
                                        int origin_z);

namespace {

void print_usage() {
    std::fputs("usage: batholith terrain [--side N] [--reps N] [--threads N] [--seed N]\n"
               "\nTimes full chunk generation and reports cubiomes' height map alongside for\n"
               "context. The two are not compared; see docs/benchmark.md.\n"
               "  --side N     region side in chunks (default 16)\n"
               "  --reps N     timed runs, best kept (default 5)\n"
               "  --threads N  worker threads (default: all cores)\n"
               "  --seed N     world seed (default 0)\n",
               stderr);
}

[[nodiscard]] double now_seconds() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

// cubiomes maps height at 1:4 horizontal resolution.
constexpr int height_scale = 4;

}  // namespace

namespace batholith {

int run_terrain(int argc, char** argv) {
    int side = 16;
    int reps = 5;
    std::size_t threads = 0;
    std::uint64_t seed = 0;
    for (int index = 1; index < argc; ++index) {
        const std::string_view flag = argv[index];
        if (flag == "--help" || flag == "-h") {
            print_usage();
            return 0;
        }
    }
    for (int index = 1; index + 1 < argc; index += 2) {
        const std::string_view flag = argv[index];
        if (flag == "--side") {
            side = std::atoi(argv[index + 1]);
        } else if (flag == "--reps") {
            reps = std::atoi(argv[index + 1]);
        } else if (flag == "--threads") {
            threads = static_cast<std::size_t>(std::strtoull(argv[index + 1], nullptr, 10));
        } else if (flag == "--seed") {
            seed = std::strtoull(argv[index + 1], nullptr, 10);
        } else {
            std::fprintf(stderr, "unknown flag: %s\n", argv[index]);
            return 2;
        }
    }
    if (side <= 0 || reps <= 0) {
        print_usage();
        return 2;
    }

    const std::size_t chunks = static_cast<std::size_t>(side) * static_cast<std::size_t>(side);
    const std::size_t height_columns =
        chunks * static_cast<std::size_t>(batholith::chunk_size_x / height_scale) *
        static_cast<std::size_t>(batholith::chunk_size_z / height_scale);
    const auto blocks =
        static_cast<long long>(chunks * static_cast<std::size_t>(batholith::blocks_per_chunk));

    const batholith::generator generator(seed);
    batholith::thread_pool pool(threads == 0 ? batholith::thread_pool::hardware_threads() : threads);

    std::vector<batholith::chunk_buffer> buffers(chunks);
    std::vector<float> heights(height_columns);

    // --- batholith surface profile, and cubiomes' depth proxy for contrast ---------
    const auto generate_chunk = [&](std::size_t index) {
        const int cx = static_cast<int>(index % static_cast<std::size_t>(side));
        const int cz = static_cast<int>(index / static_cast<std::size_t>(side));
        generator.generate(batholith::chunk_pos{cx, cz}, buffers[index]);
    };
    pool.parallel_for(chunks, generate_chunk);

    // Highest solid block per column, from the same chunk buffers the benchmark fills.
    const auto column_height = [&](std::size_t chunk_index, int x, int z) {
        for (int y = batholith::chunk_size_y - 1; y >= 0; --y) {
            const int world_y = batholith::min_y + y;
            if (batholith::is_solid(buffers[chunk_index].at(x, world_y, z))) {
                return world_y;
            }
        }
        return batholith::min_y;
    };

    std::vector<int> our_heights(height_columns);
    for (int cz = 0; cz < side; ++cz) {
        for (int cx = 0; cx < side; ++cx) {
            const std::size_t chunk_index =
                static_cast<std::size_t>(cz) * static_cast<std::size_t>(side) +
                static_cast<std::size_t>(cx);
            for (int hz = 0; hz < batholith::chunk_size_z / height_scale; ++hz) {
                for (int hx = 0; hx < batholith::chunk_size_x / height_scale; ++hx) {
                    const std::size_t column =
                        (static_cast<std::size_t>(cz) * (batholith::chunk_size_z / height_scale) +
                         static_cast<std::size_t>(hz)) *
                            (static_cast<std::size_t>(side) * (batholith::chunk_size_x / height_scale)) +
                        static_cast<std::size_t>(cx) * (batholith::chunk_size_x / height_scale) +
                        static_cast<std::size_t>(hx);
                    our_heights[column] =
                        column_height(chunk_index, hx * height_scale, hz * height_scale);
                }
            }
        }
    }

    const int height_width = side * (batholith::chunk_size_x / height_scale);
    const int height_height = side * (batholith::chunk_size_z / height_scale);
    crosscheck_approx_height(heights.data(), height_width, height_height, 0, 0);

    int our_minimum = our_heights.front();
    int our_maximum = our_heights.front();
    for (const int value : our_heights) {
        our_minimum = value < our_minimum ? value : our_minimum;
        our_maximum = value > our_maximum ? value : our_maximum;
    }
    float proxy_minimum = heights.front();
    float proxy_maximum = heights.front();
    for (const float value : heights) {
        proxy_minimum = value < proxy_minimum ? value : proxy_minimum;
        proxy_maximum = value > proxy_maximum ? value : proxy_maximum;
    }
    std::printf("surface_profiles columns=%d\n", height_width * height_height);
    std::printf("  batholith_surface   range=[%d, %d]  (real block height)\n", our_minimum,
                our_maximum);
    std::printf("  cubiomes_mapApproxHeight range=[%.1f, %.1f]  (depth proxy, NOT a block "
                "height)\n",
                static_cast<double>(proxy_minimum), static_cast<double>(proxy_maximum));

    // --- throughput ----------------------------------------------------------------
    std::printf("\nterrain_throughput chunks=%zu threads=%zu reps=%d\n", chunks, threads, reps);

    double best_batholith = 0.0;
    for (int rep = 0; rep < reps; ++rep) {
        const double start = now_seconds();
        pool.parallel_for(chunks, generate_chunk);
        const double elapsed = now_seconds() - start;
        best_batholith = (rep == 0 || elapsed < best_batholith) ? elapsed : best_batholith;
    }

    double best_cubiomes = 0.0;
    for (int rep = 0; rep < reps; ++rep) {
        const double start = now_seconds();
        crosscheck_approx_height(heights.data(), height_width, height_height, 0, 0);
        const double elapsed = now_seconds() - start;
        best_cubiomes = (rep == 0 || elapsed < best_cubiomes) ? elapsed : best_cubiomes;
    }

    const double batholith_blocks_per_second = static_cast<double>(blocks) / best_batholith;
    const double batholith_columns_per_second = static_cast<double>(height_columns) / best_batholith;
    const double cubiomes_columns_per_second =
        static_cast<double>(height_columns) / best_cubiomes;

    std::printf("  batholith  best=%.4fs  %.2f Mblocks/s  %.1f chunks/s  %.0f columns/s\n",
                best_batholith, batholith_blocks_per_second / 1e6,
                static_cast<double>(chunks) / best_batholith, batholith_columns_per_second);
    std::printf("  cubiomes   best=%.4fs  %.0f depth-samples/s  (depth proxy, not a terrain "
                "height)\n",
                best_cubiomes, cubiomes_columns_per_second);

    long long sink = 0;
    for (const int value : our_heights) {
        sink += value;
    }
    std::printf("\nchecksum=%lld\n", sink);
    return 0;
}

}  // namespace batholith
