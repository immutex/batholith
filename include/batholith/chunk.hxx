#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <batholith/block.hxx>

namespace batholith {

inline constexpr int chunk_size_x = 16;
inline constexpr int chunk_size_z = 16;
inline constexpr int chunk_size_y = 384;
inline constexpr int min_y = -64;
inline constexpr int sea_level = 63;

inline constexpr std::size_t blocks_per_chunk =
    static_cast<std::size_t>(chunk_size_x) * chunk_size_y * chunk_size_z;

inline constexpr int horizontal_cell = 4;
inline constexpr int vertical_cell = 8;
inline constexpr int lattice_x = chunk_size_x / horizontal_cell + 1;
inline constexpr int lattice_z = chunk_size_z / horizontal_cell + 1;
inline constexpr int lattice_y = chunk_size_y / vertical_cell + 1;
inline constexpr std::size_t lattice_points =
    static_cast<std::size_t>(lattice_x) * lattice_y * lattice_z;

struct chunk_pos {
    int x = 0;
    int z = 0;

    friend constexpr bool operator==(const chunk_pos&, const chunk_pos&) = default;
    friend constexpr auto operator<=>(const chunk_pos&, const chunk_pos&) = default;
};

[[nodiscard]] constexpr std::size_t index_xzy(int x, int y, int z) noexcept {
    return (static_cast<std::size_t>(y) * chunk_size_z + static_cast<std::size_t>(z)) *
               chunk_size_x +
           static_cast<std::size_t>(x);
}

class chunk_buffer {
public:
    chunk_buffer() : blocks_(blocks_per_chunk, block_id::air) {}

    [[nodiscard]] block_id at(int x, int y, int z) const noexcept {
        return blocks_[index_xzy(x, y - min_y, z)];
    }

    void set(int x, int y, int z, block_id id) noexcept {
        blocks_[index_xzy(x, y - min_y, z)] = id;
    }

    [[nodiscard]] const std::vector<block_id>& blocks() const noexcept { return blocks_; }

private:
    std::vector<block_id> blocks_;
};

[[nodiscard]] constexpr int world_min_x(const chunk_pos& pos) noexcept {
    return pos.x * chunk_size_x;
}

[[nodiscard]] constexpr int world_min_z(const chunk_pos& pos) noexcept {
    return pos.z * chunk_size_z;
}

}  // namespace batholith
