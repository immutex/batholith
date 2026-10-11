#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace batholith {

// Provides exactly `parallelism` threads: parallelism-1 workers plus the caller of
// parallel_for, which blocks until the work is done.
class thread_pool {
public:
    explicit thread_pool(std::size_t parallelism);
    ~thread_pool();

    thread_pool(const thread_pool&) = delete;
    thread_pool& operator=(const thread_pool&) = delete;
    thread_pool(thread_pool&&) = delete;
    thread_pool& operator=(thread_pool&&) = delete;

    [[nodiscard]] std::size_t size() const noexcept { return parallelism_; }

    void parallel_for(std::size_t count, const std::function<void(std::size_t)>& body);

    [[nodiscard]] static std::size_t hardware_threads() noexcept;

private:
    void worker_loop(std::size_t index);

    std::vector<std::thread> workers_;
    std::size_t parallelism_ = 1;
    std::mutex mutex_;
    std::condition_variable work_ready_;
    std::condition_variable work_done_;
    std::condition_variable start_ready_;
    const std::function<void(std::size_t)>* body_ = nullptr;
    std::size_t count_ = 0;
    std::size_t next_ = 0;
    std::size_t generation_ = 0;
    std::size_t finished_ = 0;
    bool stop_ = false;
};

}  // namespace batholith
