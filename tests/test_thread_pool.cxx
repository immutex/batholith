#include "assert.hxx"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <thread>
#include <vector>

#include <batholith/chunk_loader.hxx>
#include <batholith/thread_pool.hxx>

namespace batholith::test {
namespace {

constexpr int loader_timeout_ms = 30000;

[[nodiscard]] std::vector<built_chunk> drain(chunk_loader& loader, std::size_t expected) {
    std::vector<built_chunk> collected;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(loader_timeout_ms);
    while (collected.size() < expected && std::chrono::steady_clock::now() < deadline) {
        for (built_chunk& built : loader.take_ready()) {
            collected.push_back(std::move(built));
        }
        if (collected.size() < expected) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    return collected;
}

}  // namespace

void run_thread_pool() {
    {
        // size() must report the requested parallelism, because callers divide throughput
        // by it. A pool that silently used one extra thread made every published
        // per-core figure exactly 2x too high.
        for (const std::size_t requested : {1U, 2U, 4U, 8U}) {
            const thread_pool pool(requested);
            CHECK(pool.size() == requested);
        }
    }

    {
        // A zero request means "use the machine", which must still be a sane count.
        const thread_pool pool(0);
        CHECK(pool.size() >= 1);
        CHECK(pool.size() == thread_pool::hardware_threads());
    }

    {
        // Peak concurrency must equal size(), or throughput divided by size() is wrong.
        // Counting thread ids would miss an extra thread that never gets scheduled.
        for (const std::size_t threads : {1U, 2U, 4U}) {
            thread_pool pool(threads);
            std::atomic<int> active{0};
            std::atomic<int> peak{0};
            const std::size_t items = threads * 16;
            pool.parallel_for(items, [&active, &peak](std::size_t) {
                const int now = active.fetch_add(1, std::memory_order_relaxed) + 1;
                int seen = peak.load(std::memory_order_relaxed);
                while (now > seen &&
                       !peak.compare_exchange_weak(seen, now, std::memory_order_relaxed)) {
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                active.fetch_sub(1, std::memory_order_relaxed);
            });
            CHECK(peak.load(std::memory_order_relaxed) <= static_cast<int>(pool.size()));
            CHECK(peak.load(std::memory_order_relaxed) == static_cast<int>(pool.size()));
        }
    }

    {
        // Every index visited exactly once, at any parallelism.
        for (const std::size_t threads : {1U, 3U, 8U}) {
            thread_pool pool(threads);
            constexpr std::size_t count = 1000;
            std::vector<std::atomic<int>> visits(count);
            pool.parallel_for(count, [&visits](std::size_t index) {
                visits[index].fetch_add(1, std::memory_order_relaxed);
            });
            int correct = 0;
            for (const std::atomic<int>& visit : visits) {
                correct += visit.load(std::memory_order_relaxed) == 1 ? 1 : 0;
            }
            CHECK(correct == static_cast<int>(count));
        }
    }

    {
        // parallel_for must not return before the work is finished, which is what makes
        // its wall clock a valid measurement.
        thread_pool pool(4);
        std::atomic<int> completed{0};
        pool.parallel_for(64, [&completed](std::size_t) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            completed.fetch_add(1, std::memory_order_relaxed);
        });
        CHECK(completed.load(std::memory_order_relaxed) == 64);
    }
}

void run_chunk_loader() {
    const generator gen(0);

    {
        // Every requested position must come back exactly once, with a non-empty mesh.
        chunk_loader loader(gen, 2);
        const std::vector<chunk_pos> wanted{{0, 0}, {1, 0}, {-1, 2}, {3, -1}};
        loader.request(wanted);

        const std::vector<built_chunk> built = drain(loader, wanted.size());
        CHECK(built.size() == wanted.size());

        int matched = 0;
        for (const built_chunk& chunk : built) {
            for (const chunk_pos& position : wanted) {
                if (chunk.position == position) {
                    ++matched;
                    CHECK(!chunk.mesh.empty());
                }
            }
        }
        CHECK(matched == static_cast<int>(wanted.size()));
    }

    {
        // The loader must produce byte-identical meshes to generating inline, otherwise
        // it is not a safe substitute for the synchronous path.
        chunk_loader loader(gen, 2);
        const chunk_pos position{5, -3};
        loader.request({position});
        const std::vector<built_chunk> built = drain(loader, 1);
        CHECK(built.size() == 1);

        chunk_buffer buffer;
        gen.generate(position, buffer);
        const chunk_mesh expected = build_chunk_mesh(buffer);

        CHECK(built[0].mesh.indices == expected.indices);
        CHECK(built[0].mesh.vertices.size() == expected.vertices.size());
        bool same = built[0].mesh.vertices.size() == expected.vertices.size();
        for (std::size_t index = 0; same && index < expected.vertices.size(); ++index) {
            const mesh_vertex& a = built[0].mesh.vertices[index];
            const mesh_vertex& b = expected.vertices[index];
            same = a.x == b.x && a.y == b.y && a.z == b.z && a.r == b.r && a.g == b.g &&
                   a.b == b.b;
        }
        CHECK(same);
    }

    {
        // take_ready must not hand out the same chunk twice.
        chunk_loader loader(gen, 2);
        loader.request({{0, 0}, {1, 1}});
        const std::vector<built_chunk> first = drain(loader, 2);
        CHECK(first.size() == 2);
        CHECK(loader.take_ready().empty());
    }
}

}  // namespace batholith::test
