#ifndef SEARCHWORKER_HPP
#define SEARCHWORKER_HPP

#include <memory>
#include "search.hpp"

#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

class SearchWorker {
public:
    SearchWorker();
    ~SearchWorker();

    void set_silent(bool s);

    void go(const Board &root, const SearchLimits &limits, 
            const Stack *st = nullptr, bool ponder=false, int multipv=1);

    void stop();
    void stop_pondering();
    void wait_for_completion();

private:
    std::unique_ptr<Search> search_;

    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable go_cv_, done_cv_;

    std::atomic_bool go_, done_;
    std::atomic_bool terminate_;
};

#endif
