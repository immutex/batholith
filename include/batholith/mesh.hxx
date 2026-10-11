#pragma once

#include <cstdint>
#include <vector>

#include <batholith/block.hxx>
#include <batholith/chunk.hxx>

namespace batholith {

struct mesh_vertex {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
};

// Quads are four vertices plus six indices, so the GPU reads a third fewer vertices
// than an expanded triangle list would need.
struct chunk_mesh {
    std::vector<mesh_vertex> vertices;
    std::vector<std::uint32_t> indices;

    [[nodiscard]] bool empty() const noexcept { return indices.empty(); }
    [[nodiscard]] std::size_t quad_count() const noexcept { return indices.size() / 6; }
};

// Emits one quad per exposed face, solids and fluids alike. A face is dropped when any
// non-air neighbour hides it; out-of-chunk neighbours count as air so borders stay closed.
[[nodiscard]] chunk_mesh build_chunk_mesh(const chunk_buffer& chunk);

[[nodiscard]] std::uint32_t block_color(block_id id) noexcept;

}  // namespace batholith
