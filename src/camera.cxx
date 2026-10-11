#include <batholith/camera.hxx>

#include <cmath>

namespace batholith {
namespace {

constexpr float pitch_limit = 1.5533431F;
constexpr float field_of_view = 1.2217305F;
constexpr float near_plane = 0.1F;
constexpr float far_plane = 2000.0F;
constexpr float full_turn = 6.2831855F;

using mat4 = std::array<float, 16>;

// Keeps yaw bounded so a long session cannot lose trig precision.
[[nodiscard]] float wrap_angle(float radians) noexcept {
    return radians - full_turn * std::floor(radians / full_turn + 0.5F);
}

[[nodiscard]] mat4 multiply(const mat4& a, const mat4& b) noexcept {
    mat4 result{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0F;
            for (int k = 0; k < 4; ++k) {
                sum += a[static_cast<std::size_t>(k * 4 + row)] *
                       b[static_cast<std::size_t>(column * 4 + k)];
            }
            result[static_cast<std::size_t>(column * 4 + row)] = sum;
        }
    }
    return result;
}

[[nodiscard]] mat4 perspective(float aspect) noexcept {
    const float tangent = std::tan(field_of_view * 0.5F);
    mat4 result{};
    result[0] = 1.0F / (tangent * aspect);
    result[5] = 1.0F / tangent;
    result[10] = -(far_plane + near_plane) / (far_plane - near_plane);
    result[11] = -1.0F;
    result[14] = -(2.0F * far_plane * near_plane) / (far_plane - near_plane);
    return result;
}

[[nodiscard]] mat4 look_at(const std::array<float, 3>& eye, const std::array<float, 3>& forward,
                           const std::array<float, 3>& right,
                           const std::array<float, 3>& up) noexcept {
    mat4 result{};
    result[0] = right[0];
    result[4] = right[1];
    result[8] = right[2];
    result[1] = up[0];
    result[5] = up[1];
    result[9] = up[2];
    result[2] = -forward[0];
    result[6] = -forward[1];
    result[10] = -forward[2];
    result[12] = -(right[0] * eye[0] + right[1] * eye[1] + right[2] * eye[2]);
    result[13] = -(up[0] * eye[0] + up[1] * eye[1] + up[2] * eye[2]);
    result[14] = forward[0] * eye[0] + forward[1] * eye[1] + forward[2] * eye[2];
    result[15] = 1.0F;
    return result;
}

}  // namespace

void camera::look(float delta_yaw, float delta_pitch) noexcept {
    // Positive yaw turns right, positive pitch up, matching move() and the view matrix.
    yaw_ = wrap_angle(yaw_ + delta_yaw);
    const float pitch = pitch_ + delta_pitch;
    pitch_ = pitch < -pitch_limit ? -pitch_limit : (pitch > pitch_limit ? pitch_limit : pitch);
}

void camera::look_screen(float screen_dx, float screen_dy) noexcept {
    look(screen_dx, -screen_dy);
}

void camera::move(float forward, float right, float up, float distance) noexcept {
    const float sin_yaw = std::sin(yaw_);
    const float cos_yaw = std::cos(yaw_);
    const float sin_pitch = std::sin(pitch_);
    const float cos_pitch = std::cos(pitch_);

    const std::array<float, 3> forward_axis{sin_yaw * cos_pitch, sin_pitch, -cos_yaw * cos_pitch};
    const std::array<float, 3> right_axis{cos_yaw, 0.0F, sin_yaw};

    position_[0] += (forward_axis[0] * forward + right_axis[0] * right) * distance;
    position_[1] += (forward_axis[1] * forward + up) * distance;
    position_[2] += (forward_axis[2] * forward + right_axis[2] * right) * distance;
}

std::array<float, 16> camera::view_projection(float aspect) const noexcept {
    const float sin_yaw = std::sin(yaw_);
    const float cos_yaw = std::cos(yaw_);
    const float sin_pitch = std::sin(pitch_);
    const float cos_pitch = std::cos(pitch_);

    const std::array<float, 3> forward{sin_yaw * cos_pitch, sin_pitch, -cos_yaw * cos_pitch};
    const std::array<float, 3> right{cos_yaw, 0.0F, sin_yaw};
    const std::array<float, 3> up{
        right[1] * forward[2] - right[2] * forward[1],
        right[2] * forward[0] - right[0] * forward[2],
        right[0] * forward[1] - right[1] * forward[0],
    };

    return multiply(perspective(aspect), look_at(position_, forward, right, up));
}

}  // namespace batholith
