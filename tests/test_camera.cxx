#include "assert.hxx"

#include <cmath>

#include <batholith/camera.hxx>

namespace batholith::test {
namespace {

constexpr float pi = 3.14159265F;

// Distance moved by one unit of forward travel, which is the camera's forward axis.
[[nodiscard]] std::array<float, 3> forward_axis(camera& cam) {
    const std::array<float, 3> before = cam.position();
    cam.move(1.0F, 0.0F, 0.0F, 1.0F);
    const std::array<float, 3> after = cam.position();
    return {after[0] - before[0], after[1] - before[1], after[2] - before[2]};
}

[[nodiscard]] std::array<float, 3> right_axis(camera& cam) {
    const std::array<float, 3> before = cam.position();
    cam.move(0.0F, 1.0F, 0.0F, 1.0F);
    const std::array<float, 3> after = cam.position();
    return {after[0] - before[0], after[1] - before[1], after[2] - before[2]};
}

}  // namespace

void run_camera() {
    {
        // Yaw 0 looks down -Z, the convention the view matrix is built for.
        camera cam;
        const std::array<float, 3> forward = forward_axis(cam);
        CHECK_NEAR(forward[0], 0.0F, 1e-5);
        CHECK_NEAR(forward[1], 0.0F, 1e-5);
        CHECK_NEAR(forward[2], -1.0F, 1e-5);
    }

    {
        // Moving the mouse right must turn the view right, i.e. toward +X.
        camera cam;
        cam.look(0.5F, 0.0F);
        const std::array<float, 3> forward = forward_axis(cam);
        CHECK(forward[0] > 0.4F);
        CHECK_NEAR(cam.yaw(), 0.5F, 1e-5);
    }

    {
        // Moving the mouse left must turn the view left, i.e. toward -X.
        camera cam;
        cam.look(-0.5F, 0.0F);
        const std::array<float, 3> forward = forward_axis(cam);
        CHECK(forward[0] < -0.4F);
    }

    {
        // Dragging the pointer upward (negative screen dy) must look up.
        camera cam;
        cam.look_screen(0.0F, -0.5F);
        const std::array<float, 3> forward = forward_axis(cam);
        CHECK(forward[1] > 0.4F);
    }

    {
        // Dragging the pointer downward must look down.
        camera cam;
        cam.look_screen(0.0F, 0.5F);
        const std::array<float, 3> forward = forward_axis(cam);
        CHECK(forward[1] < -0.4F);
    }

    {
        // Dragging the pointer right must turn right.
        camera cam;
        cam.look_screen(0.5F, 0.0F);
        const std::array<float, 3> forward = forward_axis(cam);
        CHECK(forward[0] > 0.4F);
    }

    {
        // Pitch is clamped short of vertical so the look-at basis never degenerates.
        camera cam;
        for (int step = 0; step < 50; ++step) {
            cam.look(0.0F, 0.5F);
        }
        CHECK(cam.pitch() < pi * 0.5F);
        CHECK(cam.pitch() > 1.4F);
        const std::array<float, 3> forward = forward_axis(cam);
        CHECK(std::isfinite(forward[0]) && std::isfinite(forward[1]) && std::isfinite(forward[2]));
        CHECK(forward[1] > 0.9F);
    }

    {
        // Right strafing must be perpendicular to forward and horizontal.
        camera cam;
        cam.look(0.9F, 0.3F);
        const std::array<float, 3> forward = forward_axis(cam);
        const std::array<float, 3> right = right_axis(cam);
        CHECK_NEAR(right[1], 0.0F, 1e-5);
        CHECK_NEAR(forward[0] * right[0] + forward[1] * right[1] + forward[2] * right[2], 0.0F,
                   1e-5);
    }

    {
        // A long session must not let yaw grow without bound.
        camera cam;
        for (int step = 0; step < 10000; ++step) {
            cam.look(0.7F, 0.0F);
        }
        CHECK(std::fabs(cam.yaw()) <= pi + 1e-3F);
    }

    {
        // The point directly ahead must land on the screen centre.
        camera cam;
        cam.set_position({0.0F, 0.0F, 0.0F});
        const std::array<float, 16> matrix = cam.view_projection(1.0F);
        const auto clip = [&matrix](std::array<float, 3> point) {
            return std::array<float, 4>{
                matrix[0] * point[0] + matrix[4] * point[1] + matrix[8] * point[2] + matrix[12],
                matrix[1] * point[0] + matrix[5] * point[1] + matrix[9] * point[2] + matrix[13],
                matrix[2] * point[0] + matrix[6] * point[1] + matrix[10] * point[2] + matrix[14],
                matrix[3] * point[0] + matrix[7] * point[1] + matrix[11] * point[2] + matrix[15]};
        };

        const std::array<float, 4> ahead = clip({0.0F, 0.0F, -10.0F});
        CHECK(ahead[3] > 0.0F);
        CHECK_NEAR(ahead[0] / ahead[3], 0.0F, 1e-4);
        CHECK_NEAR(ahead[1] / ahead[3], 0.0F, 1e-4);

        // Negative w is what makes the rasteriser reject a point behind the camera.
        const std::array<float, 4> behind = clip({0.0F, 0.0F, 10.0F});
        CHECK(behind[3] < 0.0F);

        // Turning right must move a world point that sits on -Z to the left of centre.
        camera turned;
        turned.set_position({0.0F, 0.0F, 0.0F});
        turned.look(0.4F, 0.0F);
        const std::array<float, 16> turned_matrix = turned.view_projection(1.0F);
        const auto x_of = [&turned_matrix](std::array<float, 3> point) {
            const float x = turned_matrix[0] * point[0] + turned_matrix[4] * point[1] +
                            turned_matrix[8] * point[2] + turned_matrix[12];
            const float w = turned_matrix[3] * point[0] + turned_matrix[7] * point[1] +
                            turned_matrix[11] * point[2] + turned_matrix[15];
            return x / w;
        };
        CHECK(x_of({0.0F, 0.0F, -10.0F}) < 0.0F);
    }
}

}  // namespace batholith::test
