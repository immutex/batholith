#include <batholith/density.hxx>

#include <cmath>
#include <cstddef>

#include <batholith/math.hxx>

namespace batholith {
namespace {

constexpr double half_negative_factor = 0.5;
constexpr double quarter_negative_factor = 0.25;
constexpr double squeeze_linear = 0.5;
constexpr double squeeze_cubic = 24.0;
constexpr double shift_divisor = 4.0;

[[nodiscard]] constexpr double half_negative(double value) noexcept {
    return value < 0.0 ? value * half_negative_factor : value;
}

[[nodiscard]] constexpr double quarter_negative(double value) noexcept {
    return value < 0.0 ? value * quarter_negative_factor : value;
}

[[nodiscard]] inline double squeeze(double value) noexcept {
    const double bounded = clamp(value, -1.0, 1.0);
    return bounded * squeeze_linear - bounded * bounded * bounded / squeeze_cubic;
}

[[nodiscard]] inline double y_clamped_gradient(double y, double from_y, double to_y,
                                               double from_value, double to_value) noexcept {
    const double ratio = clamp((y - from_y) / (to_y - from_y), 0.0, 1.0);
    return lerp(ratio, from_value, to_value);
}

[[nodiscard]] constexpr std::size_t lattice_index(int x, int y, int z) noexcept {
    return (static_cast<std::size_t>(y) * lattice_z + static_cast<std::size_t>(z)) * lattice_x +
           static_cast<std::size_t>(x);
}

[[nodiscard]] constexpr std::size_t flat_index(int x, int z) noexcept {
    return static_cast<std::size_t>(z) * (chunk_size_x + 1) + static_cast<std::size_t>(x);
}

// QuantizedSpaghettiRarity step functions; the boundary value belongs to the upper branch.
[[nodiscard]] constexpr double rarity_scale_type1(double value) noexcept {
    if (value < -0.5) {
        return 0.75;
    }
    if (value < 0.0) {
        return 1.0;
    }
    return value < 0.5 ? 1.5 : 2.0;
}

[[nodiscard]] constexpr double rarity_scale_type2(double value) noexcept {
    if (value < -0.75) {
        return 0.5;
    }
    if (value < -0.5) {
        return 0.75;
    }
    if (value < 0.5) {
        return 1.0;
    }
    return value < 0.75 ? 2.0 : 3.0;
}

[[nodiscard]] inline double trilerp(double ax, double ay, double az,
                                    const double* corners) noexcept {
    return lerp(az, lerp(ay, lerp(ax, corners[0], corners[1]), lerp(ax, corners[2], corners[3])),
                lerp(ay, lerp(ax, corners[4], corners[5]), lerp(ax, corners[6], corners[7])));
}

}  // namespace

density_graph::density_graph(std::vector<density_node> nodes, std::vector<spline_point> points,
                             std::span<const int> outputs)
    : nodes_(std::move(nodes)), points_(std::move(points)) {
    prepare_order_.reserve(nodes_.size());
    std::vector<char> state(nodes_.size(), 0);
    for (const int output : outputs) {
        collect_post_order(output, state);
    }
}

// Tables must be filled children-first: vanilla nests flat_cache inside flat_cache,
// so an outer table's index can precede its inner ones.
void density_graph::collect_post_order(int node, std::vector<char>& state) {
    if (node < 0 || state[static_cast<std::size_t>(node)] != 0) {
        return;
    }
    state[static_cast<std::size_t>(node)] = 1;
    const density_node& current = nodes_[static_cast<std::size_t>(node)];
    collect_post_order(current.children[0], state);
    if (current.kind == density_kind::spline) {
        for (int offset = 0; offset < current.point_count; ++offset) {
            collect_post_order(points_[static_cast<std::size_t>(current.first_point + offset)].value,
                               state);
        }
    } else {
        collect_post_order(current.children[1], state);
        collect_post_order(current.children[2], state);
    }
    prepare_order_.push_back(node);
}

void density_graph::prepare(density_workspace& work, const noise_registry& noises, int origin_x,
                            int origin_z) const noexcept {
    work.origin_x = origin_x;
    work.origin_z = origin_z;
    work.cell.valid = false;
    work.lattice.resize(nodes_.size());
    work.flat.resize(nodes_.size());
    work.cache_value.assign(nodes_.size(), 0.0);
    work.cache_key.assign(nodes_.size(), {});
    work.cache_valid.assign(nodes_.size(), 0);

    for (const int node : prepare_order_) {
        const density_node& current = nodes_[static_cast<std::size_t>(node)];
        if (current.kind == density_kind::flat_cache) {
            std::vector<double>& table = work.flat[static_cast<std::size_t>(node)];
            table.resize(static_cast<std::size_t>(chunk_size_x + 1) * (chunk_size_z + 1));
            for (int z = 0; z <= chunk_size_z; ++z) {
                for (int x = 0; x <= chunk_size_x; ++x) {
                    table[flat_index(x, z)] =
                        evaluate(current.children[0], static_cast<double>(origin_x + x), 0.0,
                                 static_cast<double>(origin_z + z), work, noises);
                }
            }
        } else if (current.kind == density_kind::interpolated) {
            std::vector<double>& buffer = work.lattice[static_cast<std::size_t>(node)];
            buffer.resize(lattice_points);
            for (int y = 0; y < lattice_y; ++y) {
                for (int z = 0; z < lattice_z; ++z) {
                    for (int x = 0; x < lattice_x; ++x) {
                        buffer[lattice_index(x, y, z)] = evaluate(
                            current.children[0],
                            static_cast<double>(origin_x + x * horizontal_cell),
                            static_cast<double>(min_y + y * vertical_cell),
                            static_cast<double>(origin_z + z * horizontal_cell), work, noises);
                    }
                }
            }
        }
    }
}

double density_graph::sample(int node, double x, double y, double z, density_workspace& work,
                             const noise_registry& noises) const noexcept {
    return evaluate(node, x, y, z, work, noises);
}

double density_graph::sample_spline(int node, double input, double x, double y, double z,
                                    density_workspace& work,
                                    const noise_registry& noises) const noexcept {
    const density_node& current = nodes_[static_cast<std::size_t>(node)];
    const int first = current.first_point;
    const int count = current.point_count;
    const float coordinate = static_cast<float>(input);

    const auto value_at = [&](int point) {
        return static_cast<float>(
            evaluate(points_[static_cast<std::size_t>(point)].value, x, y, z, work, noises));
    };
    const auto location_at = [&](int point) {
        return static_cast<float>(points_[static_cast<std::size_t>(point)].location);
    };
    const auto derivative_at = [&](int point) {
        return static_cast<float>(points_[static_cast<std::size_t>(point)].derivative);
    };
    const auto extrapolate = [&](int point) {
        const float derivative = derivative_at(point);
        return derivative == 0.0F
                   ? value_at(point)
                   : value_at(point) + derivative * (coordinate - location_at(point));
    };

    if (coordinate < location_at(first)) {
        return static_cast<double>(extrapolate(first));
    }
    if (coordinate >= location_at(first + count - 1)) {
        return static_cast<double>(extrapolate(first + count - 1));
    }

    int low = 0;
    int high = count - 1;
    while (high - low > 1) {
        const int middle = (low + high) / 2;
        if (coordinate < location_at(first + middle)) {
            high = middle;
        } else {
            low = middle;
        }
    }

    const float x0 = location_at(first + low);
    const float x1 = location_at(first + low + 1);
    const float span = x1 - x0;
    const float t = (coordinate - x0) / span;
    const float v0 = value_at(first + low);
    const float v1 = value_at(first + low + 1);
    const float left = derivative_at(first + low) * span - (v1 - v0);
    const float right = -derivative_at(first + low + 1) * span + (v1 - v0);
    return static_cast<double>(v0 + t * (v1 - v0) +
                               t * (1.0F - t) * (left + t * (right - left)));
}

double density_graph::interpolate(int node, double x, double y, double z,
                                  density_workspace& work) const noexcept {
    trilinear_cell& cell = work.cell;
    if (!cell.valid || cell.key_x != x || cell.key_y != y || cell.key_z != z) {
        const double local_x = x - static_cast<double>(work.origin_x);
        const double local_z = z - static_cast<double>(work.origin_z);
        cell.key_x = x;
        cell.key_y = y;
        cell.key_z = z;
        cell.valid = true;
        cell.base_x =
            clamp(floor_div(floor_to_int(local_x), horizontal_cell), 0, lattice_x - 2);
        cell.base_y = clamp(floor_div(floor_to_int(y) - min_y, vertical_cell), 0, lattice_y - 2);
        cell.base_z =
            clamp(floor_div(floor_to_int(local_z), horizontal_cell), 0, lattice_z - 2);
        cell.ax = clamp((local_x - static_cast<double>(cell.base_x * horizontal_cell)) /
                            static_cast<double>(horizontal_cell),
                        0.0, 1.0);
        cell.ay = clamp((y - static_cast<double>(min_y + cell.base_y * vertical_cell)) /
                            static_cast<double>(vertical_cell),
                        0.0, 1.0);
        cell.az = clamp((local_z - static_cast<double>(cell.base_z * horizontal_cell)) /
                            static_cast<double>(horizontal_cell),
                        0.0, 1.0);
    }

    // One multiply-add per corner offset rather than a lattice_index call per corner.
    const std::vector<double>& buffer = work.lattice[static_cast<std::size_t>(node)];
    const std::size_t origin = lattice_index(cell.base_x, cell.base_y, cell.base_z);
    constexpr std::size_t y_stride = static_cast<std::size_t>(lattice_x) * lattice_z;
    constexpr std::size_t z_stride = lattice_x;
    const double corners[8] = {
        buffer[origin],
        buffer[origin + 1],
        buffer[origin + y_stride],
        buffer[origin + y_stride + 1],
        buffer[origin + z_stride],
        buffer[origin + z_stride + 1],
        buffer[origin + y_stride + z_stride],
        buffer[origin + y_stride + z_stride + 1]};
    return trilerp(cell.ax, cell.ay, cell.az, corners);
}

double density_graph::evaluate(int node, double x, double y, double z, density_workspace& work,
                               const noise_registry& noises) const noexcept {
    const density_node& current = nodes_[static_cast<std::size_t>(node)];
    const auto at = [&](int which) { return evaluate(which, x, y, z, work, noises); };

    switch (current.kind) {
        case density_kind::constant:
            return current.params[0];
        case density_kind::y_clamped_gradient:
            return y_clamped_gradient(y, current.params[0], current.params[1], current.params[2],
                                      current.params[3]);
        case density_kind::noise:
            return noises.normal(current.noise)
                .sample(x * current.params[0], y * current.params[1], z * current.params[0]);
        case density_kind::old_blended_noise:
            return noises.blended(current.noise).sample(x, y, z);
        case density_kind::shift_a:
            return noises.normal(current.noise)
                       .sample(x / shift_divisor, 0.0, z / shift_divisor) *
                   shift_divisor;
        case density_kind::shift_b:
            return noises.normal(current.noise)
                       .sample(z / shift_divisor, x / shift_divisor, 0.0) *
                   shift_divisor;
        case density_kind::shifted_noise:
            return noises.normal(current.noise)
                .sample(current.params[0] * (x + at(current.children[0])),
                        current.params[1] * (y + at(current.children[1])),
                        current.params[0] * (z + at(current.children[2])));
        case density_kind::add:
            return at(current.children[0]) + at(current.children[1]);
        case density_kind::mul:
            return at(current.children[0]) * at(current.children[1]);
        case density_kind::min:
            return std::fmin(at(current.children[0]), at(current.children[1]));
        case density_kind::max:
            return std::fmax(at(current.children[0]), at(current.children[1]));
        case density_kind::sub:
            return at(current.children[0]) - at(current.children[1]);
        case density_kind::div:
            return at(current.children[0]) / at(current.children[1]);
        case density_kind::abs:
            return std::fabs(at(current.children[0]));
        case density_kind::square: {
            const double value = at(current.children[0]);
            return value * value;
        }
        case density_kind::cube: {
            const double value = at(current.children[0]);
            return value * value * value;
        }
        case density_kind::negate:
            return -at(current.children[0]);
        case density_kind::half_negative:
            return half_negative(at(current.children[0]));
        case density_kind::quarter_negative:
            return quarter_negative(at(current.children[0]));
        case density_kind::clamp:
            return clamp(at(current.children[0]), current.params[0], current.params[1]);
        case density_kind::squeeze:
            return squeeze(at(current.children[0]));
        case density_kind::lerp:
            return lerp(at(current.children[0]), at(current.children[1]), at(current.children[2]));
        case density_kind::spline:
            return sample_spline(node, at(current.children[0]), x, y, z, work, noises);
        case density_kind::range_choice: {
            const double input = at(current.children[0]);
            return (input >= current.params[0] && input < current.params[1])
                       ? at(current.children[1])
                       : at(current.children[2]);
        }
        case density_kind::weird_scaled_sampler: {
            const double scale = current.variant == 1 ? rarity_scale_type1(at(current.children[0]))
                                                      : rarity_scale_type2(at(current.children[0]));
            return scale * std::fabs(noises.normal(current.noise)
                                         .sample(x / scale, y / scale, z / scale));
        }
        case density_kind::interpolated:
            return interpolate(node, x, y, z, work);
        case density_kind::flat_cache: {
            const int bx = floor_to_int(x) - work.origin_x;
            const int bz = floor_to_int(z) - work.origin_z;
            if (bx < 0 || bz < 0 || bx > chunk_size_x || bz > chunk_size_z) {
                return at(current.children[0]);
            }
            return work.flat[static_cast<std::size_t>(node)][flat_index(bx, bz)];
        }
        case density_kind::cache: {
            const std::size_t slot = static_cast<std::size_t>(node);
            const std::array<double, 3> key{x, y, z};
            if (work.cache_valid[slot] != 0 && work.cache_key[slot] == key) {
                return work.cache_value[slot];
            }
            const double value = at(current.children[0]);
            work.cache_key[slot] = key;
            work.cache_value[slot] = value;
            work.cache_valid[slot] = 1;
            return value;
        }
    }
    return 0.0;
}

}  // namespace batholith
