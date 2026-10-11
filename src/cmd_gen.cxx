#include "commands.hxx"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <batholith/generator.hxx>

namespace {

void print_usage() {
    std::fputs("usage: batholith gen [--seed N] [--chunks N] [--threads N] [--dump]\n"
               "\nGenerates the first `chunks` chunks of the world and prints throughput.\n"
               "  --seed N     world seed (default 0)\n"
               "  --chunks N   how many chunks to generate (default 256)\n"
               "  --threads N  worker threads (default: all cores)\n"
               "  --dump       also print the solid-block count per y level of chunk (0,0)\n",
               stderr);
}

[[nodiscard]] bool parse_int(const char* text, long long& out) {
    char* end = nullptr;
    const long long value = std::strtoll(text, &end, 10);
    if (end == text || *end != '\0') {
        return false;
    }
    out = value;
    return true;
}

}  // namespace

namespace batholith {

int run_gen(int argc, char** argv) {
    std::uint64_t seed = 0;
    long long chunks = 256;
    long long threads = 0;
    bool dump = false;

    for (int index = 1; index < argc; ++index) {
        const std::string_view arg = argv[index];
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        }
        if (arg == "--dump") {
            dump = true;
            continue;
        }
        if (index + 1 >= argc) {
            print_usage();
            return 2;
        }
        long long value = 0;
        if (!parse_int(argv[index + 1], value)) {
            print_usage();
            return 2;
        }
        ++index;
        if (arg == "--seed") {
            seed = static_cast<std::uint64_t>(value);
        } else if (arg == "--chunks") {
            chunks = value;
        } else if (arg == "--threads") {
            threads = value;
        } else {
            print_usage();
            return 2;
        }
    }

    if (chunks < 1) {
        chunks = 1;
    }
    const int side = static_cast<int>(std::lround(std::cbrt(static_cast<double>(chunks))));
    const int width = side < 1 ? 1 : side;

    const generator gen(seed);
    const std::size_t thread_count =
        threads > 0 ? static_cast<std::size_t>(threads) : thread_pool::hardware_threads();
    thread_pool pool(thread_count);
    const generation_stats stats = gen.generate_region(0, 0, width, width, pool);

    std::printf("seed=%llu chunks=%llu threads=%zu\n", static_cast<unsigned long long>(seed),
                static_cast<unsigned long long>(stats.chunks), thread_count);
    std::printf("seconds=%.4f chunks/s=%.1f blocks/s=%.1f\n", stats.seconds,
                stats.chunks_per_second(), stats.blocks_per_second());

    if (dump) {
        chunk_buffer buffer;
        gen.generate(chunk_pos{0, 0}, buffer);
        for (int y = chunk_size_y - 1; y >= 0; --y) {
            int solid = 0;
            for (int z = 0; z < chunk_size_z; ++z) {
                for (int x = 0; x < chunk_size_x; ++x) {
                    if (is_solid(buffer.at(x, min_y + y, z))) {
                        ++solid;
                    }
                }
            }
            if (solid > 0) {
                std::printf("y=%4d solid=%3d\n", min_y + y, solid);
            }
        }
    }
    return 0;
}

}  // namespace batholith
