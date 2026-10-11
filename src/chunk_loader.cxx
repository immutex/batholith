#include <batholith/chunk_loader.hxx>

#include <algorithm>
#include <utility>

namespace batholith {

chunk_loader::chunk_loader(const generator& source, std::size_t threads)
    : generator_(source), pool_(threads), worker_([this] { run(); }) {}

chunk_loader::~chunk_loader() {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    worker_.join();
}

void chunk_loader::request(std::vector<chunk_pos> wanted) {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        wanted_ = std::move(wanted);
        next_ = 0;
    }
    wake_.notify_one();
}

std::vector<built_chunk> chunk_loader::take_ready() {
    std::vector<built_chunk> taken;
    const std::lock_guard<std::mutex> lock(mutex_);
    taken.swap(ready_);
    return taken;
}

void chunk_loader::run() {
    for (;;) {
        std::vector<chunk_pos> batch;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] { return stopping_ || next_ < wanted_.size(); });
            if (stopping_) {
                return;
            }
            const std::size_t count = std::min(pool_.size(), wanted_.size() - next_);
            batch.assign(wanted_.begin() + static_cast<std::ptrdiff_t>(next_),
                         wanted_.begin() + static_cast<std::ptrdiff_t>(next_ + count));
            next_ += count;
        }

        std::vector<chunk_buffer> buffers(batch.size());
        std::vector<chunk_mesh> meshes(batch.size());
        pool_.parallel_for(batch.size(), [&](std::size_t index) {
            generator_.generate(batch[index], buffers[index]);
            meshes[index] = build_chunk_mesh(buffers[index]);
        });

        {
            const std::lock_guard<std::mutex> lock(mutex_);
            for (std::size_t index = 0; index < batch.size(); ++index) {
                ready_.push_back(built_chunk{batch[index], std::move(meshes[index])});
            }
        }
    }
}

}  // namespace batholith
