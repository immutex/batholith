#include <batholith/mesh.hxx>

#include <array>
#include <cstddef>

namespace batholith {
namespace {

struct face {
    int dx;
    int dy;
    int dz;
    float shade;
    // Counter-clockwise seen from outside the block, as GL_CCW culling requires.
    std::array<std::array<int, 3>, 4> corners;
};

constexpr std::array<face, 6> faces{{
    {0, 1, 0, 1.00F, {{{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}}}},
    {0, -1, 0, 0.45F, {{{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}}},
    {1, 0, 0, 0.78F, {{{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}}}},
    {-1, 0, 0, 0.78F, {{{0, 0, 1}, {0, 1, 1}, {0, 1, 0}, {0, 0, 0}}}},
    {0, 0, 1, 0.62F, {{{1, 0, 1}, {1, 1, 1}, {0, 1, 1}, {0, 0, 1}}}},
    {0, 0, -1, 0.62F, {{{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}}}},
}};

[[nodiscard]] bool in_chunk(int x, int y, int z) noexcept {
    return x >= 0 && x < chunk_size_x && z >= 0 && z < chunk_size_z && y >= min_y &&
           y < min_y + chunk_size_y;
}

[[nodiscard]] block_id block_or_air(const chunk_buffer& chunk, int x, int y, int z) noexcept {
    return in_chunk(x, y, z) ? chunk.at(x, y, z) : block_id::air;
}

// Any non-air neighbour hides the face; with blending, fluids would not hide solids.
[[nodiscard]] bool occludes(block_id neighbour) noexcept {
    return neighbour != block_id::air;
}

void emit_quad(chunk_mesh& mesh, const face& side, int x, int y, int z, float r, float g,
               float b) {
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    for (const std::array<int, 3>& corner : side.corners) {
        mesh.vertices.push_back(mesh_vertex{static_cast<float>(x + corner[0]),
                                            static_cast<float>(y + corner[1]),
                                            static_cast<float>(z + corner[2]), r * side.shade,
                                            g * side.shade, b * side.shade});
    }
    mesh.indices.push_back(base);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 3);
}

}  // namespace

std::uint32_t block_color(block_id id) noexcept {
    switch (id) {
        case block_id::stone: return 0x7F7F7F;
        case block_id::deepslate: return 0x4C4C50;
        case block_id::bedrock: return 0x333333;
        case block_id::dirt: return 0x8B5A2B;
        case block_id::coarse_dirt: return 0x7A4F26;
        case block_id::rooted_dirt: return 0x96683A;
        case block_id::grass_block: return 0x5E9C3F;
        case block_id::podzol: return 0x5A3D1E;
        case block_id::mycelium: return 0x6F6265;
        case block_id::sand: return 0xDBD3A0;
        case block_id::red_sand: return 0xBF6B2C;
        case block_id::sandstone: return 0xD5CC9B;
        case block_id::red_sandstone: return 0xB26228;
        case block_id::gravel: return 0x8A8683;
        case block_id::clay: return 0xA4A8B8;
        case block_id::water: return 0x3A5FCD;
        case block_id::lava: return 0xE25822;
        case block_id::snow_block: return 0xF0FAFA;
        case block_id::powder_snow: return 0xF4FBFB;
        case block_id::ice: return 0x9BC4E8;
        case block_id::packed_ice: return 0x8FB6E0;
        case block_id::blue_ice: return 0x74A2DC;
        case block_id::terracotta: return 0x985E43;
        case block_id::orange_terracotta: return 0xA15326;
        case block_id::yellow_terracotta: return 0xBA8523;
        case block_id::brown_terracotta: return 0x4D3323;
        case block_id::white_terracotta: return 0xD2B1A1;
        case block_id::light_gray_terracotta: return 0x8A7B72;
        case block_id::calcite: return 0xDFDFDF;
        case block_id::tuff: return 0x6D6D64;
        case block_id::basalt: return 0x4A4A4E;
        case block_id::blackstone: return 0x2B2528;
        case block_id::soul_soil: return 0x4B3A2A;
        case block_id::soul_sand: return 0x51413A;
        case block_id::magma_block: return 0x8E3B12;
        case block_id::netherrack: return 0x6F3634;
        case block_id::moss_block: return 0x5B7A2A;
        case block_id::muddy_mangrove_roots: return 0x4A3A2C;
        case block_id::mud: return 0x3C3139;
        case block_id::andesite: return 0x888888;
        case block_id::granite: return 0x9A6B5A;
        case block_id::diorite: return 0xBFBFBF;
        case block_id::air: break;
    }
    return 0x000000;
}

chunk_mesh build_chunk_mesh(const chunk_buffer& chunk) {
    chunk_mesh mesh;
    for (int y = 0; y < chunk_size_y; ++y) {
        const int wy = min_y + y;
        for (int z = 0; z < chunk_size_z; ++z) {
            for (int x = 0; x < chunk_size_x; ++x) {
                const block_id id = chunk.at(x, wy, z);
                if (id == block_id::air) {
                    continue;
                }
                const std::uint32_t packed = block_color(id);
                const float r = static_cast<float>((packed >> 16) & 0xFFU) / 255.0F;
                const float g = static_cast<float>((packed >> 8) & 0xFFU) / 255.0F;
                const float b = static_cast<float>(packed & 0xFFU) / 255.0F;
                for (const face& side : faces) {
                    const block_id neighbour =
                        block_or_air(chunk, x + side.dx, wy + side.dy, z + side.dz);
                    if (occludes(neighbour)) {
                        continue;
                    }
                    emit_quad(mesh, side, x, wy, z, r, g, b);
                }
            }
        }
    }
    return mesh;
}

}  // namespace batholith
