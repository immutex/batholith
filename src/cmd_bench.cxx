#include "commands.hxx"

#include <cstdio>
#include <cstdlib>
#include <string_view>

#include <batholith/generator.hxx>

namespace {

struct options {
    std::uint64_t seed = 0;
    int side = 8;
    std::size_t threads = 0;
    int repeats = 3;
    bool csv = false;
};

void print_usage() {
    std::fputs("usage: batholith bench [--seed N] [--side N] [--threads N] [--repeats N] [--csv]\n"
               "\nTimes chunk generation over a square region and reports best-of-N.\n"
               "  --seed N     world seed (default 0)\n"
               "  --side N     region side in chunks, so side^2 chunks (default 8)\n"
               "  --threads N  worker threads (default: all cores)\n"
               "  --repeats N  timed runs, best kept (default 3)\n"
               "  --csv        emit one CSV row instead of the readable report\n",
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

[[nodiscard]] bool parse(int argc, char** argv, options& opts) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view arg = argv[index];
        if (arg == "--csv") {
            opts.csv = true;
            continue;
        }
        if (arg == "--help" || arg == "-h") {
            print_usage();
            std::exit(0);
        }
        if (index + 1 >= argc) {
            return false;
        }
        long long value = 0;
        if (!parse_int(argv[index + 1], value)) {
            return false;
        }
        ++index;
        if (arg == "--seed") {
            opts.seed = static_cast<std::uint64_t>(value);
        } else if (arg == "--side") {
            opts.side = static_cast<int>(value);
        } else if (arg == "--threads") {
            opts.threads = static_cast<std::size_t>(value);
        } else if (arg == "--repeats") {
            opts.repeats = static_cast<int>(value);
        } else {
            return false;
        }
    }
    return true;
}

}  // namespace

namespace batholith {

int run_bench(int argc, char** argv) {
    options opts;
    if (!parse(argc, argv, opts)) {
        print_usage();
        return 2;
    }
    if (opts.side < 1) {
        opts.side = 1;
    }
    if (opts.repeats < 1) {
        opts.repeats = 1;
    }

    const std::size_t threads =
        opts.threads > 0 ? opts.threads : thread_pool::hardware_threads();
    thread_pool pool(threads);
    const generator gen(opts.seed);

    generation_stats best;
    for (int run = 0; run < opts.repeats; ++run) {
        const generation_stats stats = gen.generate_region(0, 0, opts.side, opts.side, pool);
        if (stats.seconds < best.seconds || best.seconds == 0.0) {
            best = stats;
        }
    }

    const double per_core_blocks = best.blocks_per_second() / static_cast<double>(threads);

    if (opts.csv) {
        std::printf("engine,seed,side,threads,chunks,blocks,seconds,chunks_per_second,"
                    "blocks_per_second,blocks_per_second_per_core\n");
        std::printf("batholith,%llu,%d,%zu,%llu,%llu,%.6f,%.2f,%.2f,%.2f\n",
                    static_cast<unsigned long long>(opts.seed), opts.side, threads,
                    static_cast<unsigned long long>(best.chunks),
                    static_cast<unsigned long long>(best.blocks), best.seconds,
                    best.chunks_per_second(), best.blocks_per_second(), per_core_blocks);
        return 0;
    }

    std::printf("batholith seed=%llu side=%d threads=%zu\n",
                static_cast<unsigned long long>(opts.seed), opts.side, threads);
    std::printf("  chunks        %llu\n", static_cast<unsigned long long>(best.chunks));
    std::printf("  blocks        %llu\n", static_cast<unsigned long long>(best.blocks));
    std::printf("  seconds       %.4f\n", best.seconds);
    std::printf("  chunks/s      %.1f\n", best.chunks_per_second());
    std::printf("  blocks/s      %.1f\n", best.blocks_per_second());
    std::printf("  blocks/s/core %.1f\n", per_core_blocks);
    return 0;
}

}  // namespace batholith
