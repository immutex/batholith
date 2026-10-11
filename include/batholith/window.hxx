#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

#include <batholith/gl.hxx>

namespace batholith {

struct key_state {
    std::array<bool, 256> down{};

    [[nodiscard]] bool operator[](int virtual_key) const noexcept {
        return down[static_cast<std::size_t>(virtual_key)];
    }
};

class window {
public:
    window(int width, int height, std::string_view title);
    ~window();

    window(const window&) = delete;
    window& operator=(const window&) = delete;
    window(window&&) = delete;
    window& operator=(window&&) = delete;

    // Returns false once the user closes the window.
    [[nodiscard]] bool pump_events() noexcept;

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] const key_state& keys() const noexcept { return keys_; }

    // Consumes pointer motion accumulated since the previous call.
    [[nodiscard]] std::pair<float, float> take_mouse_delta() noexcept;
    [[nodiscard]] bool cursor_captured() const noexcept { return cursor_captured_; }

    void set_cursor_captured(bool captured) noexcept;
    void set_title(std::string_view title);
    void swap_buffers() noexcept;
    void request_close() noexcept { close_requested_ = true; }

    [[nodiscard]] LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) noexcept;

private:
    void create_context();
    void accumulate_mouse(LPARAM lparam) noexcept;

    HWND handle_ = nullptr;
    HDC device_ = nullptr;
    HGLRC context_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    key_state keys_{};
    float mouse_dx_ = 0.0F;
    float mouse_dy_ = 0.0F;
    bool close_requested_ = false;
    bool cursor_captured_ = false;
};

}  // namespace batholith
