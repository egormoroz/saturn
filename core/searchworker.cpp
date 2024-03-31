#include "searchworker.hpp"

SearchWorker::SearchWorker() 
    : search_(new Search)
{
    terminate_.store(false, std::memory_order_relaxed);
    go_.store(false, std::memory_order_relaxed);
    done_.store(true, std::memory_order_relaxed);

    thread_ = std::thread([this] {
        while (true) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                go_cv_.wait(lock, [this] { return go_ || terminate_; });
            }

            if (terminate_) break;

            search_->iterative_deepening();

            go_ = false;
            done_ = true;
            done_cv_.notify_all();
        }
    });
}

SearchWorker::~SearchWorker() {
    stop();
    wait_for_completion();
    thread_.join();
}

void SearchWorker::set_silent(bool s) {
    search_->set_silent(s);
}

void SearchWorker::go(const Board &root, const SearchLimits &limits, 
        const Stack *st, bool ponder, int multipv) 
{
    stop();
    wait_for_completion();

    search_->setup(root, limits, st, ponder, multipv);

    done_ = false;
    go_ = true;
    go_cv_.notify_one();
}

void SearchWorker::stop() {
    search_->atomic_stop();
    go_ = false;
}

void SearchWorker::stop_pondering() {
    search_->stop_pondering();
}

void SearchWorker::wait_for_completion() {
    std::unique_lock<std::mutex> lock(mutex_);
    done_cv_.wait(lock, [this] { return done_.load(std::memory_order_relaxed); });
}


