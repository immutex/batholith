#include <batholith/gl.hxx>

namespace batholith::gl {
namespace {

[[nodiscard]] void* resolve(const char* name) {
    void* loaded = reinterpret_cast<void*>(wglGetProcAddress(name));
    if (loaded != nullptr) {
        return loaded;
    }
    const HMODULE module = LoadLibraryA("opengl32.dll");
    return module == nullptr ? nullptr : reinterpret_cast<void*>(GetProcAddress(module, name));
}

template <typename T>
void bind(T& target, const char* name) {
    target = reinterpret_cast<T>(resolve(name));
}

}  // namespace

const api& functions() {
    static const api table = [] {
        api loaded;
        bind(loaded.gen_vertex_arrays, "glGenVertexArrays");
        bind(loaded.bind_vertex_array, "glBindVertexArray");
        bind(loaded.delete_vertex_arrays, "glDeleteVertexArrays");
        bind(loaded.gen_buffers, "glGenBuffers");
        bind(loaded.bind_buffer, "glBindBuffer");
        bind(loaded.buffer_data, "glBufferData");
        bind(loaded.delete_buffers, "glDeleteBuffers");
        bind(loaded.enable_vertex_attrib_array, "glEnableVertexAttribArray");
        bind(loaded.vertex_attrib_pointer, "glVertexAttribPointer");
        bind(loaded.create_shader, "glCreateShader");
        bind(loaded.shader_source, "glShaderSource");
        bind(loaded.compile_shader, "glCompileShader");
        bind(loaded.get_shader_iv, "glGetShaderiv");
        bind(loaded.get_shader_info_log, "glGetShaderInfoLog");
        bind(loaded.delete_shader, "glDeleteShader");
        bind(loaded.create_program, "glCreateProgram");
        bind(loaded.attach_shader, "glAttachShader");
        bind(loaded.link_program, "glLinkProgram");
        bind(loaded.get_program_iv, "glGetProgramiv");
        bind(loaded.get_program_info_log, "glGetProgramInfoLog");
        bind(loaded.use_program, "glUseProgram");
        bind(loaded.delete_program, "glDeleteProgram");
        bind(loaded.get_uniform_location, "glGetUniformLocation");
        bind(loaded.uniform_matrix4fv, "glUniformMatrix4fv");
        bind(loaded.uniform3fv, "glUniform3fv");
        bind(loaded.draw_elements, "glDrawElements");
        return loaded;
    }();
    return table;
}

}  // namespace batholith::gl
