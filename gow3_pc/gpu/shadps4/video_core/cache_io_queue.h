// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>

namespace Storage {
// Single consumer, concurrent producers. Queue and wake predicate share one
// lock: no separately incremented/decremented request count can lose a wake.
class CacheIOQueue {
public:
    using Task = std::packaged_task<void()>;
    // Called before starting a consumer, after the previous consumer was joined.
    void Open() {
        std::lock_guard lock{mutex};
        closed = false;
    }
    bool Submit(Task task) {
        {
            std::lock_guard lock{mutex};
            if (closed) return false;
            tasks.push(std::move(task));
        }
        ready.notify_one();
        return true;
    }
    void Close() {
        {
            std::lock_guard lock{mutex};
            closed = true;
        }
        ready.notify_all();
    }
    void Run() {
        for (;;) {
            Task task;
            {
                std::unique_lock lock{mutex};
                ready.wait(lock, [&] { return closed || !tasks.empty(); });
                if (tasks.empty()) return;
                task = std::move(tasks.front());
                tasks.pop();
            }
            task(); // packaged_task captures errors; execution is already synchronous
        }
    }
private:
    std::mutex mutex;
    std::condition_variable ready;
    std::queue<Task> tasks;
    bool closed = true;
};
} // namespace Storage
