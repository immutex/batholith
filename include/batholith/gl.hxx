#pragma once

#if !defined(_WIN32)
#error "batholith viewer currently supports Windows only"
#endif

#include <cstddef>

#include <windows.h>

#include <GL/gl.h>

// The Windows SDK ships only GL 1.1 headers and MSVC has no glext.h, so the modern entry
// points are declared here, keeping the viewer dependency-free.
namespace batholith::gl {

using gl_char = char;
using gl_sizeiptr = std::ptrdiff_t;

inline constexpr GLenum array_buffer = 0x8892;
inline constexpr GLenum element_array_buffer = 0x8893;
inline constexpr GLenum unsigned_int = 0x1405;
inline constexpr GLenum static_draw = 0x88E4;
inline constexpr GLenum vertex_shader = 0x8B31;
inline constexpr GLenum fragment_shader = 0x8B30;
inline constexpr GLenum compile_status = 0x8B81;
inline constexpr GLenum link_status = 0x8B82;

using gen_vertex_arrays_fn = void(APIENTRY*)(GLsizei, GLuint*);
using bind_vertex_array_fn = void(APIENTRY*)(GLuint);
using delete_vertex_arrays_fn = void(APIENTRY*)(GLsizei, const GLuint*);
using gen_buffers_fn = void(APIENTRY*)(GLsizei, GLuint*);
using bind_buffer_fn = void(APIENTRY*)(GLenum, GLuint);
using buffer_data_fn = void(APIENTRY*)(GLenum, gl_sizeiptr, const void*, GLenum);
using delete_buffers_fn = void(APIENTRY*)(GLsizei, const GLuint*);
using enable_vertex_attrib_array_fn = void(APIENTRY*)(GLuint);
using vertex_attrib_pointer_fn = void(APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei,
                                                const void*);
using create_shader_fn = GLuint(APIENTRY*)(GLenum);
using shader_source_fn = void(APIENTRY*)(GLuint, GLsizei, const gl_char* const*, const GLint*);
using compile_shader_fn = void(APIENTRY*)(GLuint);
using get_shader_iv_fn = void(APIENTRY*)(GLuint, GLenum, GLint*);
using get_shader_info_log_fn = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, gl_char*);
using delete_shader_fn = void(APIENTRY*)(GLuint);
using create_program_fn = GLuint(APIENTRY*)();
using attach_shader_fn = void(APIENTRY*)(GLuint, GLuint);
using link_program_fn = void(APIENTRY*)(GLuint);
using get_program_iv_fn = void(APIENTRY*)(GLuint, GLenum, GLint*);
using get_program_info_log_fn = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, gl_char*);
using use_program_fn = void(APIENTRY*)(GLuint);
using delete_program_fn = void(APIENTRY*)(GLuint);
using get_uniform_location_fn = GLint(APIENTRY*)(GLuint, const gl_char*);
using uniform_matrix4fv_fn = void(APIENTRY*)(GLint, GLsizei, GLboolean, const GLfloat*);
using uniform3fv_fn = void(APIENTRY*)(GLint, GLsizei, const GLfloat*);
using draw_elements_fn = void(APIENTRY*)(GLenum, GLsizei, GLenum, const void*);

// OpenGL 1.1 entry points are exported by opengl32.dll; everything newer is resolved
// at runtime through wglGetProcAddress.
struct api {
    gen_vertex_arrays_fn gen_vertex_arrays = nullptr;
    bind_vertex_array_fn bind_vertex_array = nullptr;
    delete_vertex_arrays_fn delete_vertex_arrays = nullptr;
    gen_buffers_fn gen_buffers = nullptr;
    bind_buffer_fn bind_buffer = nullptr;
    buffer_data_fn buffer_data = nullptr;
    delete_buffers_fn delete_buffers = nullptr;
    enable_vertex_attrib_array_fn enable_vertex_attrib_array = nullptr;
    vertex_attrib_pointer_fn vertex_attrib_pointer = nullptr;
    create_shader_fn create_shader = nullptr;
    shader_source_fn shader_source = nullptr;
    compile_shader_fn compile_shader = nullptr;
    get_shader_iv_fn get_shader_iv = nullptr;
    get_shader_info_log_fn get_shader_info_log = nullptr;
    delete_shader_fn delete_shader = nullptr;
    create_program_fn create_program = nullptr;
    attach_shader_fn attach_shader = nullptr;
    link_program_fn link_program = nullptr;
    get_program_iv_fn get_program_iv = nullptr;
    get_program_info_log_fn get_program_info_log = nullptr;
    use_program_fn use_program = nullptr;
    delete_program_fn delete_program = nullptr;
    get_uniform_location_fn get_uniform_location = nullptr;
    uniform_matrix4fv_fn uniform_matrix4fv = nullptr;
    uniform3fv_fn uniform3fv = nullptr;
    draw_elements_fn draw_elements = nullptr;
};

// Resolved on first use; a context must be current.
[[nodiscard]] const api& functions();

}  // namespace batholith::gl
