#pragma once

#include <array>
#include <cstddef>

namespace batholith {

// Yaw and pitch in radians; yaw 0 looks down -Z, pitch positive looks up.
class camera {
public:
    camera() = default;

    // Look deltas: positive turns right and looks up.
    void look(float delta_yaw, float delta_pitch) noexcept;

    // Pointer deltas in screen space, where y grows downward.
    void look_screen(float screen_dx, float screen_dy) noexcept;
    void move(float forward, float right, float up, float distance) noexcept;

    [[nodiscard]] std::array<float, 3> position() const noexcept { return position_; }
    [[nodiscard]] float yaw() const noexcept { return yaw_; }
    [[nodiscard]] float pitch() const noexcept { return pitch_; }

    void set_position(std::array<float, 3> position) noexcept { position_ = position; }

    [[nodiscard]] std::array<float, 16> view_projection(float aspect) const noexcept;

private:
    std::array<float, 3> position_{0.0F, 100.0F, 0.0F};
    float yaw_ = 0.0F;
    float pitch_ = 0.0F;
};

}  // namespace batholith
