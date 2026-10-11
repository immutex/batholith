cmake_minimum_required(VERSION 3.25)

project(batholith
    VERSION 0.1.0
    DESCRIPTION "Minecraft-compatible world generation, rebuilt for speed"
    LANGUAGES CXX C)

option(BATHOLITH_BUILD_TESTS "Build the test suite" ON)
option(BATHOLITH_BUILD_COMPETITORS
    "Add the competitor subcommands when their third_party checkouts are present" ON)
option(BATHOLITH_NATIVE "Optimize for the host CPU" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_library(batholith_warnings INTERFACE)
add_library(batholith::warnings ALIAS batholith_warnings)

if(MSVC)
    target_compile_options(batholith_warnings INTERFACE
        /W4 /permissive- /Zc:__cplusplus /Zc:preprocessor /utf-8 /EHsc
        /wd4244 /wd4267)
else()
    target_compile_options(batholith_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough
        -Wnon-virtual-dtor -Wold-style-cast -Wcast-qual -Wcast-align
        -Wunused -Woverloaded-virtual -Wnull-dereference -Wundef
        -Wswitch-enum -Wzero-as-null-pointer-constant)
endif()

add_library(batholith_optimization INTERFACE)
add_library(batholith::optimization ALIAS batholith_optimization)

target_compile_options(batholith_optimization INTERFACE
    $<$<CONFIG:Release>:
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -fno-math-errno>
        $<$<CXX_COMPILER_ID:MSVC>:/O2 /fp:precise>>
    $<$<CONFIG:RelWithDebInfo>:
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O2 -fno-math-errno>>
    $<$<CONFIG:Debug>:
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O1>>)

if(BATHOLITH_NATIVE AND NOT MSVC)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-march=native" BATHOLITH_HAS_MARCH_NATIVE)
    if(BATHOLITH_HAS_MARCH_NATIVE)
        target_compile_options(batholith_optimization INTERFACE -march=native)
    endif()
endif()

find_package(Threads REQUIRED)

add_library(batholith
    src/block.cxx
    src/rng.cxx
    src/noise.cxx
    src/density.cxx
    src/router.cxx
    src/router_data.cxx
    src/aquifer.cxx
    src/surface.cxx
    src/generator.cxx
    src/thread_pool.cxx
    src/camera.cxx
    src/chunk_loader.cxx
    src/mesh.cxx)

target_include_directories(batholith PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(batholith
    PUBLIC Threads::Threads
    PRIVATE batholith::warnings batholith::optimization)

# One tool, one executable. `gen` and `bench` are always present; the rest are compiled
# in only when their dependency exists on the target platform, which is what the
# BATHOLITH_WITH_* guards in src/commands.hxx key off.
add_executable(batholith_tool
    src/main.cxx
    src/cmd_gen.cxx
    src/cmd_bench.cxx)
set_target_properties(batholith_tool PROPERTIES OUTPUT_NAME batholith)
target_link_libraries(batholith_tool
    PRIVATE batholith batholith::warnings batholith::optimization)

if(WIN32)
    add_library(batholith_viewer_lib
        src/gl.cxx
        src/window.cxx)
    target_link_libraries(batholith_viewer_lib
        PUBLIC batholith
        PRIVATE batholith::warnings batholith::optimization opengl32)

    target_sources(batholith_tool PRIVATE src/cmd_view.cxx)
    target_link_libraries(batholith_tool PRIVATE batholith_viewer_lib)
    target_compile_definitions(batholith_tool PRIVATE BATHOLITH_WITH_VIEWER)
endif()

if(BATHOLITH_BUILD_COMPETITORS)
    # The competitor harnesses need the gitignored third_party checkouts, so they are built
    # only when those are present. See docs/competitors.md for the fetch commands.
    #
    # The third_party libraries are built with the MinGW toolchain, so the competitor
    # harnesses are restricted to GCC/Clang. Mixing a MinGW static library into an MSVC
    # link is not supported.
    set(BATHOLITH_CUBIOMES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cubiomes)
    set(BATHOLITH_FASTNOISE2_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/FastNoise2)
    set(BATHOLITH_FASTNOISE2_LIB
        ${BATHOLITH_FASTNOISE2_DIR}/build/Release/lib/libFastNoise.a)

    if(NOT MSVC AND EXISTS ${BATHOLITH_CUBIOMES_DIR}/libcubiomes.a)
        # The bridges are C because cubiomes declares a plain lerp() that collides with
        # std::lerp. They are a separate target so the project's C++ warning set, which is
        # not valid for C, and the third-party headers, are both kept out of the harness
        # translation units.
        add_library(batholith_competitor_bridges STATIC
            src/bridges/height_bridge.c
            src/bridges/dpn_bridge.c)
        target_include_directories(batholith_competitor_bridges SYSTEM PRIVATE third_party)
        set_target_properties(batholith_competitor_bridges PROPERTIES
            C_STANDARD 11 C_STANDARD_REQUIRED ON)

        target_sources(batholith_tool PRIVATE src/cmd_terrain.cxx)
        target_link_libraries(batholith_tool PRIVATE
            batholith_competitor_bridges ${BATHOLITH_CUBIOMES_DIR}/libcubiomes.a)
        target_compile_definitions(batholith_tool PRIVATE BATHOLITH_WITH_CUBIOMES)

        # libFastNoise.a is a C++ library built with MinGW GCC, so it can only be linked
        # by a compiler sharing that C++ runtime. llvm-mingw defaults to libc++ and MSVC
        # to its own STL, so this one harness is GNU-only. The cubiomes harness links a
        # pure C library and is not restricted.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND EXISTS ${BATHOLITH_FASTNOISE2_LIB})
            target_sources(batholith_tool PRIVATE src/cmd_noise.cxx)
            target_link_libraries(batholith_tool PRIVATE ${BATHOLITH_FASTNOISE2_LIB})
            # FastNoise2 needs both include roots; the generated FastSIMD config is
            # internal to the library. SYSTEM keeps its warnings out of our build.
            target_include_directories(batholith_tool SYSTEM PRIVATE
                ${BATHOLITH_FASTNOISE2_DIR}/include
                ${BATHOLITH_FASTNOISE2_DIR}/build/_deps/fastsimd-src/include)
            target_compile_definitions(batholith_tool PRIVATE
                BATHOLITH_WITH_FASTNOISE2 FASTNOISE_STATIC_LIB FASTSIMD_STATIC_LIB)
        endif()
    endif()
endif()

if(BATHOLITH_BUILD_TESTS)
    enable_testing()
    add_executable(batholith_tests
        tests/test_main.cxx
        tests/test_camera.cxx
        tests/test_thread_pool.cxx
        tests/test_chunk.cxx
        tests/test_mesh.cxx
        tests/test_noise.cxx
        tests/test_density.cxx
        tests/test_generator.cxx)
    target_link_libraries(batholith_tests PRIVATE batholith batholith::warnings batholith::optimization)
    target_include_directories(batholith_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME batholith_tests COMMAND batholith_tests)
endif()
