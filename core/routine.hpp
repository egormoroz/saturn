#ifndef ROUTINE_HPP
#define ROUTINE_HPP

#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>

class Routine {
public:
    Routine() = default;

    void start(std::function<void()> f) {
        thread_ = std::thread([this, f]() {
            while (!terminate_.load(std::memory_order_relaxed)) {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() { 
                    return go_.load(std::memory_order_relaxed) 
                        || terminate_.load(std::memory_order_relaxed);
                });
                if (go_.load(std::memory_order_relaxed))
                    f();
                go_.store(false, std::memory_order_relaxed);
            }
        });
    }

    bool keep_going() const { 
        return go_.load(std::memory_order_relaxed);
    }

    void resume() {
        go_.store(true, std::memory_order_relaxed);
        cv_.notify_one();
    }

    void pause() {
        go_.store(false, std::memory_order_relaxed);
    }

    void terminate() {
        go_.store(false, std::memory_order_relaxed);
        terminate_.store(true, std::memory_order_relaxed);
        cv_.notify_one();
    }

    void wait_for_completion() {
        std::lock_guard<std::mutex> lck(mutex_);
    }

    void join() {
        if (thread_.joinable())
            thread_.join();
    }

    ~Routine() {
        terminate();
        join();
    }

private:
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;

    std::atomic_bool go_;
    std::atomic_bool terminate_;
};

#endif
