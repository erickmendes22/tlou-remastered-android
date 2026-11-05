#include "../gpu/shadps4/video_core/cache_io_queue.h"
#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

int main() {
    Storage::CacheIOQueue queue;
    constexpr int producers = 8, each = 2000;
    std::vector<std::atomic<unsigned>> seen(producers * each);
    queue.Open();
    std::thread consumer([&] { queue.Run(); });
    std::vector<std::thread> writers;
    for (int p = 0; p < producers; ++p) writers.emplace_back([&, p] {
        for (int i = 0; i < each; ++i) {
            const int id = p * each + i;
            assert(queue.Submit(Storage::CacheIOQueue::Task{[&, id] { ++seen[id]; }}));
        }
    });
    for (auto& writer : writers) writer.join();
    std::vector<uint8_t> payload(4 * 1024 * 1024, 0x5a);
    const auto* original = payload.data();
    auto moved = Storage::CacheIOQueue::Task{[data = std::move(payload), original] {
        assert(data.data() == original && data.front() == 0x5a && data.back() == 0x5a);
    }};
    auto complete = moved.get_future();
    assert(queue.Submit(std::move(moved)));
    auto failing = Storage::CacheIOQueue::Task{[] { throw std::runtime_error("I/O failure"); }};
    auto failure = failing.get_future();
    assert(queue.Submit(std::move(failing)));
    queue.Close(); // drains accepted writes even when stop is requested with a backlog
    consumer.join();
    complete.get();
    try { failure.get(); assert(false); } catch (const std::runtime_error&) {}
    for (const auto& count : seen) assert(count == 1);
    assert(!queue.Submit(Storage::CacheIOQueue::Task{[] { assert(false); }}));
    queue.Open(); // a later game session can reuse the queue
    unsigned reopened = 0;
    assert(queue.Submit(Storage::CacheIOQueue::Task{[&] { ++reopened; }}));
    queue.Close();
    queue.Run();
    assert(reopened == 1);
    queue.Open();
    unsigned raii_drained = 0;
    {
        std::jthread worker([&](std::stop_token token) {
            std::stop_callback stop{token, [&] { queue.Close(); }};
            queue.Run();
        });
        assert(queue.Submit(Storage::CacheIOQueue::Task{[&] { ++raii_drained; }}));
    }
    assert(raii_drained == 1);
    std::cout << "cache-io-queue: 16000 concurrent writes, move-only payload, errors, close/drain and reopen passed\n";
}
