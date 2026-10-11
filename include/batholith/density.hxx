#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <batholith/chunk.hxx>
#include <batholith/noise.hxx>

namespace batholith {

enum class density_kind : std::uint8_t {
    constant,
    y_clamped_gradient,
    noise,
    shifted_noise,
    old_blended_noise,
    shift_a,
    shift_b,
    add,
    mul,
    min,
    max,
    sub,
    div,
    abs,
    square,
    cube,
    negate,
    half_negative,
    quarter_negative,
    clamp,
    squeeze,
    lerp,
    spline,
    range_choice,
    weird_scaled_sampler,
    interpolated,
    flat_cache,
    cache,
};

struct density_node {
    density_kind kind = density_kind::constant;
    std::array<int, 3> children{-1, -1, -1};
    int noise = -1;
    int first_point = -1;
    int point_count = 0;
    int variant = 0;
    std::array<double, 4> params{0.0, 0.0, 0.0, 0.0};
};

struct spline_point {
    double location = 0.0;
    double derivative = 0.0;
    int value = -1;
};

class noise_registry {
public:
    void add(normal_noise noise) { normals_.push_back(std::move(noise)); }
    void add(blended_noise noise) { blended_.push_back(std::move(noise)); }

    [[nodiscard]] const normal_noise& normal(int index) const noexcept {
        return normals_[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] const blended_noise& blended(int index) const noexcept {
        return blended_[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] std::size_t size() const noexcept { return normals_.size() + blended_.size(); }

private:
    std::vector<normal_noise> normals_;
    std::vector<blended_noise> blended_;
};

struct trilinear_cell {
    double key_x = 0.0;
    double key_y = 0.0;
    double key_z = 0.0;
    bool valid = false;
    int base_x = 0;
    int base_y = 0;
    int base_z = 0;
    double ax = 0.0;
    double ay = 0.0;
    double az = 0.0;
};

struct density_workspace {
    // Lattice and flat tables are chunk-local, so the sampler needs the chunk origin to
    // convert the world coordinates it is called with.
    int origin_x = 0;
    int origin_z = 0;
    std::vector<std::vector<double>> lattice;
    std::vector<std::vector<double>> flat;
    std::vector<double> cache_value;
    std::vector<std::array<double, 3>> cache_key;
    std::vector<char> cache_valid;
    // Depends only on the sample position, shared by every interpolated node for that
    // block, so caching it removes repeated work.
    trilinear_cell cell;
};

class density_graph {
public:
    density_graph(std::vector<density_node> nodes, std::vector<spline_point> points,
                  std::span<const int> outputs);

    void prepare(density_workspace& work, const noise_registry& noises, int origin_x,
                 int origin_z) const noexcept;

    [[nodiscard]] double sample(int node, double x, double y, double z, density_workspace& work,
                                const noise_registry& noises) const noexcept;

    // Nodes reachable from the prepared outputs. Smaller than the table size because
    // unreachable subtrees are never ordered and never evaluated.
    [[nodiscard]] std::size_t reachable_node_count() const noexcept {
        return prepare_order_.size();
    }

private:
    [[nodiscard]] double evaluate(int node, double x, double y, double z, density_workspace& work,
                                  const noise_registry& noises) const noexcept;

    [[nodiscard]] double sample_spline(int node, double input, double x, double y, double z,
                                       density_workspace& work,
                                       const noise_registry& noises) const noexcept;

    [[nodiscard]] double interpolate(int node, double x, double y, double z,
                                     density_workspace& work) const noexcept;

    void collect_post_order(int node, std::vector<char>& state);

    std::vector<density_node> nodes_;
    std::vector<spline_point> points_;
    std::vector<int> prepare_order_;
};

}  // namespace batholith
