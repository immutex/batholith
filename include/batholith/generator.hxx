#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <batholith/aquifer.hxx>
#include <batholith/block.hxx>
#include <batholith/chunk.hxx>
#include <batholith/density.hxx>
#include <batholith/router.hxx>
#include <batholith/surface.hxx>
#include <batholith/thread_pool.hxx>

namespace batholith {

struct generation_stats {
    std::uint64_t chunks = 0;
    std::uint64_t blocks = 0;
    double seconds = 0.0;

    [[nodiscard]] double chunks_per_second() const noexcept {
        return seconds > 0.0 ? static_cast<double>(chunks) / seconds : 0.0;
    }

    [[nodiscard]] double blocks_per_second() const noexcept {
        return seconds > 0.0 ? static_cast<double>(blocks) / seconds : 0.0;
    }
};

class generator {
public:
    explicit generator(std::uint64_t seed);

    void generate(const chunk_pos& pos, chunk_buffer& out) const;

    [[nodiscard]] generation_stats generate_region(int origin_x, int origin_z, int width, int depth,
                                                   thread_pool& pool) const;
    [[nodiscard]] generation_stats generate_region(int origin_x, int origin_z, int width,
                                                   int depth) const;

private:
    void generate_into(const chunk_pos& pos, chunk_buffer& out, density_workspace& work) const
        noexcept;

    world_router router_;
    aquifer aquifer_;
};

}  // namespace batholith
