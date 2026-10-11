#include "commands.hxx"

#include <cstdio>
#include <string_view>

namespace {

void print_usage() {
    std::fputs("usage: batholith <command> [options]\n\ncommands:\n", stderr);
    for (const batholith::command& entry : batholith::commands()) {
        std::fprintf(stderr, "  %-10s %s\n", entry.name, entry.summary);
    }
    std::fputs("\nRun `batholith <command> --help` for that command's options.\n", stderr);
}

}  // namespace

namespace batholith {

std::span<const command> commands() {
    static constexpr command table[] = {
        {"gen", "generate chunks and report throughput", run_gen},
        {"bench", "time chunk generation over a region", run_bench},
#if defined(BATHOLITH_WITH_VIEWER)
        {"view", "open the interactive GL viewer", run_view},
#endif
#if defined(BATHOLITH_WITH_CUBIOMES)
        {"terrain", "compare chunk generation against cubiomes", run_terrain},
#endif
#if defined(BATHOLITH_WITH_FASTNOISE2)
        {"noise", "compare the climate noise against cubiomes and FastNoise2", run_noise},
#endif
    };
    return table;
}

}  // namespace batholith

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 2;
    }
    const std::string_view requested = argv[1];
    if (requested == "--help" || requested == "-h" || requested == "help") {
        print_usage();
        return 0;
    }
    for (const batholith::command& entry : batholith::commands()) {
        if (requested == entry.name) {
            return entry.run(argc - 1, argv + 1);
        }
    }
    std::fprintf(stderr, "unknown command: %s\n\n", argv[1]);
    print_usage();
    return 2;
}
