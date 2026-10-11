#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include <batholith/camera.hxx>
#include <batholith/chunk_loader.hxx>
#include <batholith/generator.hxx>
#include <batholith/mesh.hxx>
#include <batholith/window.hxx>

#include "commands.hxx"

namespace {

constexpr int window_width = 1280;
constexpr int window_height = 720;
constexpr int view_radius = 8;
constexpr float mouse_sensitivity = 0.0022F;
constexpr float walk_speed = 12.0F;
constexpr float fly_speed = 60.0F;
constexpr float eye_height = 1.7F;
constexpr int target_fps = 60;

const char* vertex_shader_source = R"(#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
uniform mat4 view_projection;
uniform vec3 chunk_origin;
out vec3 vertex_color;
void main() {
    vertex_color = color;
    // Meshes are built in chunk-local space so they can be cached; place them here.
    gl_Position = view_projection * vec4(position + chunk_origin, 1.0);
}
)";

const char* fragment_shader_source = R"(#version 330 core
in vec3 vertex_color;
out vec4 frag_color;
void main() {
    frag_color = vec4(vertex_color, 1.0);
}
)";

struct options {
    std::uint64_t seed = 0;
    int frames = 0;
    std::string screenshot;
    float walk_seconds = 0.0F;
    bool no_capture = false;
};

void print_usage() {
    std::fputs("usage: batholith view [--seed N] [--frames N] [--screenshot PATH]\n"
               "                     [--walk SECONDS] [--no-capture]\n"
               "\nInteractive GL viewer. WASD to move, mouse to look, Space/Shift to rise\n"
               "and sink, Ctrl to fly.\n"
               "  --seed N          world seed (default 0)\n"
               "  --frames N        render N frames then exit (default: run until closed)\n"
               "  --walk SECONDS    walk forward automatically for the first N seconds\n"
               "  --screenshot PATH write the final frame as a PPM\n"
               "  --no-capture      do not grab the cursor, and fail if the frame is flat\n",
               stderr);
}

[[nodiscard]] bool parse(int argc, char** argv, options& out) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view arg = argv[index];
        if (arg == "--no-capture") {
            out.no_capture = true;
            continue;
        }
        if (arg == "--help" || arg == "-h") {
            print_usage();
            std::exit(0);
        }
        if (index + 1 >= argc) {
            return false;
        }
        const std::string_view value = argv[index + 1];
        ++index;
        if (arg == "--seed") {
            out.seed = std::strtoull(value.data(), nullptr, 10);
        } else if (arg == "--frames") {
            out.frames = std::atoi(value.data());
        } else if (arg == "--screenshot") {
            out.screenshot = std::string(value);
        } else if (arg == "--walk") {
            out.walk_seconds = static_cast<float>(std::atof(value.data()));
        } else {
            return false;
        }
    }
    return true;
}

[[nodiscard]] GLuint compile(GLenum stage, const char* source) {
    const batholith::gl::api& gl = batholith::gl::functions();
    const GLuint shader = gl.create_shader(stage);
    gl.shader_source(shader, 1, &source, nullptr);
    gl.compile_shader(shader);
    GLint status = GL_FALSE;
    gl.get_shader_iv(shader, batholith::gl::compile_status, &status);
    if (status == GL_FALSE) {
        char log[1024] = {};
        gl.get_shader_info_log(shader, sizeof(log) - 1, nullptr, log);
        std::fprintf(stderr, "shader compile failed: %s\n", log);
        gl.delete_shader(shader);
        return 0;
    }
    return shader;
}

[[nodiscard]] GLuint build_program() {
    const batholith::gl::api& gl = batholith::gl::functions();
    const GLuint vertex = compile(batholith::gl::vertex_shader, vertex_shader_source);
    const GLuint fragment = compile(batholith::gl::fragment_shader, fragment_shader_source);
    if (vertex == 0 || fragment == 0) {
        return 0;
    }
    const GLuint program = gl.create_program();
    gl.attach_shader(program, vertex);
    gl.attach_shader(program, fragment);
    gl.link_program(program);
    gl.delete_shader(vertex);
    gl.delete_shader(fragment);

    GLint status = GL_FALSE;
    gl.get_program_iv(program, batholith::gl::link_status, &status);
    if (status == GL_FALSE) {
        char log[1024] = {};
        gl.get_program_info_log(program, sizeof(log) - 1, nullptr, log);
        std::fprintf(stderr, "program link failed: %s\n", log);
        gl.delete_program(program);
        return 0;
    }
    return program;
}

struct gpu_mesh {
    GLuint vao = 0;
    GLuint vertex_buffer = 0;
    GLuint index_buffer = 0;
    GLsizei index_count = 0;
};

[[nodiscard]] gpu_mesh upload(const batholith::chunk_mesh& mesh) {
    const batholith::gl::api& gl = batholith::gl::functions();
    gpu_mesh uploaded;
    if (mesh.empty()) {
        return uploaded;
    }
    uploaded.index_count = static_cast<GLsizei>(mesh.indices.size());

    gl.gen_vertex_arrays(1, &uploaded.vao);
    gl.bind_vertex_array(uploaded.vao);

    gl.gen_buffers(1, &uploaded.vertex_buffer);
    gl.bind_buffer(batholith::gl::array_buffer, uploaded.vertex_buffer);
    gl.buffer_data(batholith::gl::array_buffer,
                   static_cast<batholith::gl::gl_sizeiptr>(
                       mesh.vertices.size() * sizeof(batholith::mesh_vertex)),
                   mesh.vertices.data(), batholith::gl::static_draw);

    gl.gen_buffers(1, &uploaded.index_buffer);
    gl.bind_buffer(batholith::gl::element_array_buffer, uploaded.index_buffer);
    gl.buffer_data(batholith::gl::element_array_buffer,
                   static_cast<batholith::gl::gl_sizeiptr>(
                       mesh.indices.size() * sizeof(std::uint32_t)),
                   mesh.indices.data(), batholith::gl::static_draw);

    constexpr GLsizei stride = sizeof(batholith::mesh_vertex);
    gl.enable_vertex_attrib_array(0);
    gl.vertex_attrib_pointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    gl.enable_vertex_attrib_array(1);
    gl.vertex_attrib_pointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                             reinterpret_cast<const void*>(3 * sizeof(float)));
    gl.bind_vertex_array(0);
    return uploaded;
}

void release(const gpu_mesh& mesh) {
    const batholith::gl::api& gl = batholith::gl::functions();
    if (mesh.index_buffer != 0) {
        gl.delete_buffers(1, &mesh.index_buffer);
    }
    if (mesh.vertex_buffer != 0) {
        gl.delete_buffers(1, &mesh.vertex_buffer);
    }
    if (mesh.vao != 0) {
        gl.delete_vertex_arrays(1, &mesh.vao);
    }
}

[[nodiscard]] bool write_ppm(const std::string& path, int width, int height) {
    const std::size_t row_bytes = static_cast<std::size_t>(width) * 3;
    std::vector<unsigned char> pixels(row_bytes * static_cast<std::size_t>(height));
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    file << "P6\n" << width << ' ' << height << "\n255\n";
    // glReadPixels returns bottom-up rows; PPM wants top-down.
    for (int row = height - 1; row >= 0; --row) {
        file.write(reinterpret_cast<const char*>(
                       pixels.data() + static_cast<std::size_t>(row) * row_bytes),
                   static_cast<std::streamsize>(row_bytes));
    }
    return file.good();
}

// Rejects a frame that is a single flat colour, which is what a failed draw looks like.
[[nodiscard]] bool frame_is_interesting(int width, int height) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) *
                                      static_cast<std::size_t>(height) * 3);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    const unsigned char* first = pixels.data();
    std::size_t distinct = 0;
    for (std::size_t index = 0; index < pixels.size(); index += 3) {
        if (pixels[index] != first[0] || pixels[index + 1] != first[1] ||
            pixels[index + 2] != first[2]) {
            ++distinct;
        }
    }
    return distinct > pixels.size() / 100;
}

// Highest solid block in the column, so the camera spawns above terrain, not inside it.
[[nodiscard]] int surface_height(const batholith::chunk_buffer& chunk, int x, int z) {
    for (int y = batholith::chunk_size_y - 1; y >= 0; --y) {
        const int world_y = batholith::min_y + y;
        if (batholith::is_solid(chunk.at(x, world_y, z))) {
            return world_y;
        }
    }
    return batholith::sea_level;
}

}  // namespace

namespace batholith {

int run_view(int argc, char** argv) {
    options opts;
    if (!parse(argc, argv, opts)) {
        print_usage();
        return 2;
    }

    try {
        batholith::window window(window_width, window_height, "batholith");

        const GLuint program = build_program();
        if (program == 0) {
            return 1;
        }
        const batholith::gl::api& gl = batholith::gl::functions();
        const GLint matrix_location = gl.get_uniform_location(program, "view_projection");
        const GLint origin_location = gl.get_uniform_location(program, "chunk_origin");

        std::fprintf(stdout, "renderer: %s\n", glGetString(GL_RENDERER));
        std::fprintf(stdout, "version:  %s\n", glGetString(GL_VERSION));

        const batholith::generator gen(opts.seed);
        batholith::chunk_loader loader(gen, batholith::thread_pool::hardware_threads());

        batholith::camera camera;
        {
            batholith::chunk_buffer spawn_chunk;
            gen.generate(batholith::chunk_pos{0, 0}, spawn_chunk);
            const float ground = static_cast<float>(surface_height(spawn_chunk, 8, 8));
            camera.set_position({8.0F, ground + eye_height, 8.0F});
            camera.look(0.0F, -0.10F);
        }

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glClearColor(0.53F, 0.72F, 0.92F, 1.0F);

        std::map<batholith::chunk_pos, gpu_mesh> meshes;
        std::array<int, 2> loaded_centre{0, 0};
        bool centre_valid = false;
        int frames_drawn = 0;
        float walked = 0.0F;
        bool interesting = true;
        std::vector<batholith::chunk_pos> pending;

        const bool interactive = opts.frames == 0;
        if (interactive) {
            window.set_cursor_captured(true);
        }

        auto previous = std::chrono::steady_clock::now();
        while (window.pump_events()) {
            const auto now = std::chrono::steady_clock::now();
            const float elapsed = std::chrono::duration<float>(now - previous).count();
            previous = now;

            // Consumed even when not captured, so a paused window injects no stale motion.
            const auto [mouse_dx, mouse_dy] = window.take_mouse_delta();
            if (window.cursor_captured()) {
                camera.look_screen(mouse_dx * mouse_sensitivity, mouse_dy * mouse_sensitivity);
            }

            const batholith::key_state& keys = window.keys();
            float forward = static_cast<float>(keys['W']) - static_cast<float>(keys['S']);
            const float strafe = static_cast<float>(keys['D']) - static_cast<float>(keys['A']);
            const float vertical =
                static_cast<float>(keys[VK_SPACE]) - static_cast<float>(keys[VK_SHIFT]);
            if (opts.walk_seconds > 0.0F && walked < opts.walk_seconds) {
                forward += 1.0F;
                walked += elapsed;
            }
            const float speed = keys[VK_CONTROL] ? fly_speed : walk_speed;
            camera.move(forward, strafe, vertical, speed * elapsed);

            const std::array<float, 3> position = camera.position();
            const int centre_x =
                static_cast<int>(std::floor(position[0] / batholith::chunk_size_x));
            const int centre_z =
                static_cast<int>(std::floor(position[2] / batholith::chunk_size_z));

            // A batch may land after the view moved; drop what is now out of radius.
            for (batholith::built_chunk& built : loader.take_ready()) {
                if (std::abs(built.position.x - centre_x) > view_radius ||
                    std::abs(built.position.z - centre_z) > view_radius) {
                    continue;
                }
                const gpu_mesh uploaded = upload(built.mesh);
                const auto existing = meshes.find(built.position);
                if (existing != meshes.end()) {
                    release(existing->second);
                    existing->second = uploaded;
                } else {
                    meshes.emplace(built.position, uploaded);
                }
            }

            if (!centre_valid || centre_x != loaded_centre[0] || centre_z != loaded_centre[1]) {
                loaded_centre = {centre_x, centre_z};
                centre_valid = true;

                std::vector<batholith::chunk_pos> wanted;
                wanted.reserve(static_cast<std::size_t>(2 * view_radius + 1) *
                               static_cast<std::size_t>(2 * view_radius + 1));
                for (int dz = -view_radius; dz <= view_radius; ++dz) {
                    for (int dx = -view_radius; dx <= view_radius; ++dx) {
                        const batholith::chunk_pos candidate{centre_x + dx, centre_z + dz};
                        if (!meshes.contains(candidate)) {
                            wanted.push_back(candidate);
                        }
                    }
                }
                // Nearest chunks first, so terrain fills in around the camera.
                std::sort(wanted.begin(), wanted.end(),
                          [centre_x, centre_z](const batholith::chunk_pos& a,
                                               const batholith::chunk_pos& b) {
                              const int da = std::abs(a.x - centre_x) + std::abs(a.z - centre_z);
                              const int db = std::abs(b.x - centre_x) + std::abs(b.z - centre_z);
                              return da < db;
                          });
                loader.request(std::move(wanted));

                for (auto it = meshes.begin(); it != meshes.end();) {
                    const int dx = it->first.x - centre_x;
                    const int dz = it->first.z - centre_z;
                    if (dx < -view_radius || dx > view_radius || dz < -view_radius ||
                        dz > view_radius) {
                        release(it->second);
                        it = meshes.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glViewport(0, 0, window.width(), window.height());
            gl.use_program(program);

            const float aspect =
                static_cast<float>(window.width()) / static_cast<float>(window.height());
            const std::array<float, 16> matrix = camera.view_projection(aspect);
            gl.uniform_matrix4fv(matrix_location, 1, GL_FALSE, matrix.data());

            for (const auto& [chunk_position, mesh] : meshes) {
                if (mesh.index_count == 0) {
                    continue;
                }
                const std::array<float, 3> origin{
                    static_cast<float>(chunk_position.x * batholith::chunk_size_x), 0.0F,
                    static_cast<float>(chunk_position.z * batholith::chunk_size_z)};
                gl.uniform3fv(origin_location, 1, origin.data());
                gl.bind_vertex_array(mesh.vao);
                gl.draw_elements(GL_TRIANGLES, mesh.index_count, batholith::gl::unsigned_int,
                                 nullptr);
            }
            gl.bind_vertex_array(0);

            ++frames_drawn;
            if (interactive) {
                char title[96] = {};
                std::snprintf(title, sizeof(title), "batholith  %d chunks  %.0f fps  (%.0f, %.0f)",
                              static_cast<int>(meshes.size()),
                              static_cast<double>(1.0F / elapsed),
                              static_cast<double>(position[0]),
                              static_cast<double>(position[2]));
                window.set_title(title);
            }

            const bool last_frame = opts.frames > 0 && frames_drawn >= opts.frames;
            if (last_frame) {
                glFinish();
                interesting = frame_is_interesting(window.width(), window.height());
                if (!opts.screenshot.empty()) {
                    const bool written =
                        write_ppm(opts.screenshot, window.width(), window.height());
                    std::fprintf(stdout, "screenshot: %s (%s)\n", opts.screenshot.c_str(),
                                 written ? "ok" : "failed");
                }
            }

            window.swap_buffers();
            if (last_frame) {
                break;
            }

            const auto frame_end = std::chrono::steady_clock::now();
            const auto frame_time =
                std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - now);
            const auto budget = std::chrono::milliseconds(1000 / target_fps);
            if (frame_time < budget) {
                Sleep(static_cast<DWORD>((budget - frame_time).count()));
            }
        }

        for (const auto& [chunk_position, mesh] : meshes) {
            release(mesh);
        }
        gl.delete_program(program);

        if (opts.frames > 0) {
            std::fprintf(stdout, "frames=%d distinct=%s\n", frames_drawn,
                         interesting ? "yes" : "no");
            return interesting ? 0 : 1;
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "error: %s\n", error.what());
        return 1;
    }
    return 0;
}

}  // namespace batholith
