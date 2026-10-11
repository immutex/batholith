#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

#include <batholith/chunk.hxx>
#include <batholith/generator.hxx>
#include <batholith/mesh.hxx>
#include <batholith/thread_pool.hxx>

namespace batholith {

struct built_chunk {
    chunk_pos position;
    chunk_mesh mesh;
};

// Generates and meshes chunks on a worker thread so a render loop never stalls waiting for
// worldgen. The worker touches no GL state; the caller uploads from take_ready().
class chunk_loader {
public:
    explicit chunk_loader(const generator& source, std::size_t threads);
    ~chunk_loader();

    chunk_loader(const chunk_loader&) = delete;
    chunk_loader& operator=(const chunk_loader&) = delete;
    chunk_loader(chunk_loader&&) = delete;
    chunk_loader& operator=(chunk_loader&&) = delete;

    // Replaces the queue. Positions already in the caller's cache must be filtered out
    // by the caller; anything left queued but omitted here is dropped.
    void request(std::vector<chunk_pos> wanted);

    // Moves out everything finished since the previous call; never blocks.
    [[nodiscard]] std::vector<built_chunk> take_ready();

private:
    void run();

    const generator& generator_;
    thread_pool pool_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::vector<chunk_pos> wanted_;
    std::vector<built_chunk> ready_;
    std::size_t next_ = 0;
    bool stopping_ = false;
    std::thread worker_;
};

}  // namespace batholith
