#pragma once

#include <cstdint>
#include <vector>

#include <batholith/density.hxx>
#include <batholith/router_data.hxx>

namespace batholith {

class world_router {
public:
    explicit world_router(std::uint64_t seed);

    [[nodiscard]] const density_graph& graph() const noexcept { return graph_; }
    [[nodiscard]] const noise_registry& noises() const noexcept { return noises_; }

    [[nodiscard]] int node(router_output which) const noexcept {
        return data_.outputs[static_cast<std::size_t>(which)];
    }

    // Nodes evaluated for the prepared outputs, after unreachable subtrees are pruned.
    // Exposed because it is the observable behind the pruning claim in the docs.
    [[nodiscard]] std::size_t prepared_node_count() const noexcept {
        return graph_.reachable_node_count();
    }

private:
    [[nodiscard]] std::vector<int> prepared_node_indices() const;

    router_data data_;
    noise_registry noises_;
    density_graph graph_;
};

}  // namespace batholith
