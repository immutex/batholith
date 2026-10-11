#include <batholith/window.hxx>

#include <stdexcept>
#include <string>
#include <utility>

LRESULT CALLBACK window_procedure(HWND, UINT, WPARAM, LPARAM);

namespace {

constexpr wchar_t window_class[] = L"batholith";

void register_class() {
    static const bool registered = [] {
        WNDCLASSEXW description{};
        description.cbSize = sizeof(description);
        description.style = CS_OWNDC;
        description.lpfnWndProc = window_procedure;
        description.hInstance = GetModuleHandleW(nullptr);
        description.lpszClassName = window_class;
        description.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        return RegisterClassExW(&description) != 0;
    }();
    if (!registered) {
        throw std::runtime_error("failed to register window class");
    }
}

}  // namespace

LRESULT CALLBACK window_procedure(HWND handle, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* self = reinterpret_cast<batholith::window*>(GetWindowLongPtrW(handle, GWLP_USERDATA));
    if (self == nullptr) {
        return DefWindowProcW(handle, message, wparam, lparam);
    }
    return self->handle_message(message, wparam, lparam);
}

namespace batholith {

LRESULT window::handle_message(UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    switch (message) {
        case WM_CLOSE:
            request_close();
            return 0;
        case WM_SIZE:
            width_ = LOWORD(lparam);
            height_ = HIWORD(lparam);
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wparam < keys_.down.size()) {
                keys_.down[wparam] = true;
            }
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (wparam < keys_.down.size()) {
                keys_.down[wparam] = false;
            }
            return 0;
        case WM_MOUSEMOVE:
            if (cursor_captured_) {
                accumulate_mouse(lparam);
            }
            return 0;
        default:
            return DefWindowProcW(handle_, message, wparam, lparam);
    }
}

void window::accumulate_mouse(LPARAM lparam) noexcept {
    const int x = static_cast<short>(LOWORD(lparam));
    const int y = static_cast<short>(HIWORD(lparam));
    const int centre_x = width_ / 2;
    const int centre_y = height_ / 2;
    mouse_dx_ += static_cast<float>(x - centre_x);
    mouse_dy_ += static_cast<float>(y - centre_y);
    POINT centre{centre_x, centre_y};
    ClientToScreen(handle_, &centre);
    SetCursorPos(centre.x, centre.y);
}

window::window(int width, int height, std::string_view title) : width_(width), height_(height) {
    register_class();

    const std::wstring wide_title(title.begin(), title.end());
    handle_ = CreateWindowExW(0, window_class, wide_title.c_str(), WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr,
                              GetModuleHandleW(nullptr), nullptr);
    if (handle_ == nullptr) {
        throw std::runtime_error("failed to create window");
    }
    SetWindowLongPtrW(handle_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    ShowWindow(handle_, SW_SHOW);

    RECT client{};
    GetClientRect(handle_, &client);
    width_ = client.right - client.left;
    height_ = client.bottom - client.top;
    create_context();
}

window::~window() {
    if (context_ != nullptr) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(context_);
    }
    if (device_ != nullptr && handle_ != nullptr) {
        ReleaseDC(handle_, device_);
    }
    if (handle_ != nullptr) {
        DestroyWindow(handle_);
    }
}

void window::create_context() {
    device_ = GetDC(handle_);

    PIXELFORMATDESCRIPTOR format{};
    format.nSize = sizeof(format);
    format.nVersion = 1;
    format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    format.iPixelType = PFD_TYPE_RGBA;
    format.cColorBits = 32;
    format.cDepthBits = 24;
    const int chosen = ChoosePixelFormat(device_, &format);
    if (chosen == 0 || SetPixelFormat(device_, chosen, &format) == 0) {
        throw std::runtime_error("failed to configure a pixel format");
    }

    const HGLRC legacy = wglCreateContext(device_);
    if (legacy == nullptr) {
        throw std::runtime_error("failed to create a GL context");
    }
    wglMakeCurrent(device_, legacy);

    using create_context_attribs_fn = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
    // wglGetProcAddress returns an untyped PROC; route through void* to convert legally.
    void* const entry = reinterpret_cast<void*>(wglGetProcAddress("wglCreateContextAttribsARB"));
    const auto create_attribs = reinterpret_cast<create_context_attribs_fn>(entry);

    if (create_attribs != nullptr) {
        constexpr int context_major_version = 0x2091;
        constexpr int context_minor_version = 0x2092;
        constexpr int context_profile_mask = 0x9126;
        constexpr int context_core_profile = 0x00000001;
        const int attributes[] = {context_major_version, 3, context_minor_version, 3,
                                  context_profile_mask, context_core_profile, 0};
        context_ = create_attribs(device_, nullptr, attributes);
    }
    if (context_ != nullptr) {
        wglMakeCurrent(device_, context_);
        wglDeleteContext(legacy);
    } else {
        context_ = legacy;
    }
}

bool window::pump_events() noexcept {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return !close_requested_;
}

std::pair<float, float> window::take_mouse_delta() noexcept {
    const std::pair<float, float> delta{mouse_dx_, mouse_dy_};
    mouse_dx_ = 0.0F;
    mouse_dy_ = 0.0F;
    return delta;
}

void window::set_cursor_captured(bool captured) noexcept {
    if (captured == cursor_captured_) {
        return;
    }
    cursor_captured_ = captured;
    ShowCursor(captured ? FALSE : TRUE);
    if (captured) {
        POINT centre{width_ / 2, height_ / 2};
        ClientToScreen(handle_, &centre);
        SetCursorPos(centre.x, centre.y);
    }
}

void window::set_title(std::string_view title) {
    const std::wstring wide(title.begin(), title.end());
    SetWindowTextW(handle_, wide.c_str());
}

void window::swap_buffers() noexcept {
    SwapBuffers(device_);
}

}  // namespace batholith
