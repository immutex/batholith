#pragma once

#include <span>

namespace batholith {

struct command {
    const char* name;
    const char* summary;
    int (*run)(int argc, char** argv);
};

int run_gen(int argc, char** argv);
int run_bench(int argc, char** argv);

[[nodiscard]] std::span<const command> commands();

// Present only when the target platform has a GL context, which today means Windows.
#if defined(BATHOLITH_WITH_VIEWER)
int run_view(int argc, char** argv);
#endif

// Present only when the gitignored third_party checkouts needed by each harness exist.
#if defined(BATHOLITH_WITH_CUBIOMES)
int run_terrain(int argc, char** argv);
#endif
#if defined(BATHOLITH_WITH_FASTNOISE2)
int run_noise(int argc, char** argv);
#endif

}  // namespace batholith
