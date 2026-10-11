#include "assert.hxx"
#include "test.hxx"

#include <cstdio>

int main() {
    std::printf("chunk\n");
    std::fflush(stdout);
    batholith::test::run_chunk();
    std::printf("noise\n");
    std::fflush(stdout);
    batholith::test::run_noise();
    std::printf("density\n");
    std::fflush(stdout);
    batholith::test::run_mesh();
    batholith::test::run_camera();
    batholith::test::run_thread_pool();
    batholith::test::run_chunk_loader();
    batholith::test::run_density();
    std::printf("generator\n");
    std::fflush(stdout);
    batholith::test::run_generator();
    return batholith::test::summary();
}
