#include "assert.hxx"

#include <array>

#include <batholith/mesh.hxx>

namespace batholith::test {
namespace {

[[nodiscard]] chunk_buffer single_block(block_id id) {
    chunk_buffer chunk;
    chunk.set(8, 0, 8, id);
    return chunk;
}

// Fills an axis-aligned box in world coordinates, clipped to the chunk.
[[nodiscard]] chunk_buffer box(int x0, int x1, int y0, int y1, int z0, int z1, block_id id) {
    chunk_buffer chunk;
    for (int x = x0; x <= x1; ++x) {
        for (int y = y0; y <= y1; ++y) {
            for (int z = z0; z <= z1; ++z) {
                chunk.set(x, y, z, id);
            }
        }
    }
    return chunk;
}

// Faces a rectangular box exposes, counting each unit face once.
[[nodiscard]] constexpr std::size_t box_surface_faces(int sx, int sy, int sz) noexcept {
    const auto x = static_cast<std::size_t>(sx);
    const auto y = static_cast<std::size_t>(sy);
    const auto z = static_cast<std::size_t>(sz);
    return 2 * (x * y + x * z + y * z);
}

}  // namespace

void run_mesh() {
    {
        const chunk_mesh mesh = build_chunk_mesh(single_block(block_id::stone));
        CHECK(mesh.quad_count() == 6);
        CHECK(mesh.indices.size() == 6 * 6);
        CHECK(mesh.vertices.size() == 6 * 4);
        for (const mesh_vertex& vertex : mesh.vertices) {
            CHECK(vertex.x >= 8.0F && vertex.x <= 9.0F);
            CHECK(vertex.y >= 0.0F && vertex.y <= 1.0F);
            CHECK(vertex.z >= 8.0F && vertex.z <= 9.0F);
        }
    }

    CHECK(build_chunk_mesh(single_block(block_id::air)).empty());

    {
        // A lone water block is visible; it is not air and has no fluid neighbour.
        const chunk_mesh mesh = build_chunk_mesh(single_block(block_id::water));
        CHECK(mesh.quad_count() == 6);
    }

    {
        // A 16x16x4 slab: interior faces must be culled, leaving exactly the surface.
        const chunk_mesh mesh =
            build_chunk_mesh(box(0, 15, min_y, min_y + 3, 0, 15, block_id::stone));
        CHECK(mesh.quad_count() == box_surface_faces(16, 4, 16));
    }

    {
        // A 3x3x3 cube: the centre block contributes nothing.
        const chunk_mesh mesh = build_chunk_mesh(box(7, 9, -1, 1, 7, 9, block_id::stone));
        CHECK(mesh.quad_count() == box_surface_faces(3, 3, 3));
    }

    {
        // A solid 16x16x16 region fully inside the chunk must expose only its shell.
        const chunk_mesh mesh =
            build_chunk_mesh(box(0, 15, min_y, min_y + 15, 0, 15, block_id::stone));
        CHECK(mesh.quad_count() == box_surface_faces(16, 16, 16));
    }

    {
        // Two adjacent blocks share a face: 12 total minus the 2 hidden = 10.
        chunk_buffer chunk;
        chunk.set(8, 0, 8, block_id::stone);
        chunk.set(9, 0, 8, block_id::stone);
        CHECK(build_chunk_mesh(chunk).quad_count() == 10);
    }

    {
        // Water over stone: the stone's top face is still drawn because fluid does not
        // occlude solid, and the shared water/water faces cancel out.
        chunk_buffer chunk;
        chunk.set(8, 0, 8, block_id::stone);
        chunk.set(8, 1, 8, block_id::water);
        // stone 6 + water 6 - 2 hidden (stone top under water, water bottom over stone) = 10.
        CHECK(build_chunk_mesh(chunk).quad_count() == 10);
    }

    {
        // A 2x1x1 run of water hides its two shared faces: 12 - 2 = 10.
        chunk_buffer chunk;
        chunk.set(8, 0, 8, block_id::water);
        chunk.set(9, 0, 8, block_id::water);
        CHECK(build_chunk_mesh(chunk).quad_count() == 10);
    }

    // Each quad must be wound CCW seen from outside, or back-face culling removes it:
    // the cross product of the first two edges must point away from the block centre.
    {
        chunk_buffer chunk;
        chunk.set(8, 0, 8, block_id::stone);
        const chunk_mesh mesh = build_chunk_mesh(chunk);
        constexpr std::array<float, 3> centre{8.5F, 0.5F, 8.5F};
        for (std::size_t quad = 0; quad < mesh.quad_count(); ++quad) {
            const std::uint32_t base = mesh.indices[quad * 6];
            const mesh_vertex& a = mesh.vertices[base];
            const mesh_vertex& b = mesh.vertices[mesh.indices[quad * 6 + 1]];
            const mesh_vertex& c = mesh.vertices[mesh.indices[quad * 6 + 2]];
            const std::array<float, 3> edge1{b.x - a.x, b.y - a.y, b.z - a.z};
            const std::array<float, 3> edge2{c.x - a.x, c.y - a.y, c.z - a.z};
            const std::array<float, 3> normal{edge1[1] * edge2[2] - edge1[2] * edge2[1],
                                              edge1[2] * edge2[0] - edge1[0] * edge2[2],
                                              edge1[0] * edge2[1] - edge1[1] * edge2[0]};
            const std::array<float, 3> outward{a.x - centre[0], a.y - centre[1],
                                               a.z - centre[2]};
            CHECK(normal[0] * outward[0] + normal[1] * outward[1] + normal[2] * outward[2] >
                  0.0F);
        }
    }

    CHECK(block_color(block_id::stone) != block_color(block_id::dirt));
    CHECK(block_color(block_id::air) == 0);
}

}  // namespace batholith::test
