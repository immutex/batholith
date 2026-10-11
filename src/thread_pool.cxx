#include <batholith/thread_pool.hxx>

#include <algorithm>

namespace batholith {

std::size_t thread_pool::hardware_threads() noexcept {
    const unsigned detected = std::thread::hardware_concurrency();
    return detected == 0 ? 1U : static_cast<std::size_t>(detected);
}

thread_pool::thread_pool(std::size_t parallelism) {
    parallelism_ = parallelism == 0 ? hardware_threads() : parallelism;
    // The caller supplies the last thread, so parallelism 1 needs no worker at all.
    const std::size_t workers = parallelism_ - 1;
    workers_.reserve(workers);
    for (std::size_t index = 0; index < workers; ++index) {
        workers_.emplace_back([this, index] { worker_loop(index); });
    }
}

thread_pool::~thread_pool() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    work_ready_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void thread_pool::parallel_for(std::size_t count, const std::function<void(std::size_t)>& body) {
    if (count == 0) {
        return;
    }
    if (parallelism_ == 1 || count == 1) {
        for (std::size_t index = 0; index < count; ++index) {
            body(index);
        }
        return;
    }

    {
        std::unique_lock lock(mutex_);
        body_ = &body;
        count_ = count;
        next_ = 0;
        finished_ = 0;
        ++generation_;
    }
    work_ready_.notify_all();

    // The calling thread is one of the pool's threads, so this loop is what makes the
    // total exactly parallelism_, not parallelism_ + 1.
    for (;;) {
        std::size_t index = 0;
        {
            std::lock_guard lock(mutex_);
            if (next_ >= count_) {
                break;
            }
            index = next_++;
        }
        body(index);
    }

    std::unique_lock lock(mutex_);
    work_done_.wait(lock, [this] { return finished_ >= workers_.size(); });
    body_ = nullptr;
    count_ = 0;
}

void thread_pool::worker_loop(std::size_t index) {
    (void)index;
    std::size_t seen_generation = 0;
    for (;;) {
        const std::function<void(std::size_t)>* body = nullptr;
        std::size_t total = 0;
        {
            std::unique_lock lock(mutex_);
            work_ready_.wait(lock, [this, &seen_generation] {
                return stop_ || generation_ != seen_generation;
            });
            if (stop_) {
                return;
            }
            seen_generation = generation_;
            body = body_;
            total = count_;
        }

        for (;;) {
            std::size_t item = 0;
            {
                std::lock_guard lock(mutex_);
                if (next_ >= total) {
                    break;
                }
                item = next_++;
            }
            (*body)(item);
        }

        {
            std::lock_guard lock(mutex_);
            ++finished_;
        }
        work_done_.notify_one();
    }
}

}  // namespace batholith
