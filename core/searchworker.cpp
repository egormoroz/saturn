#include "searchworker.hpp"

SearchWorker::SearchWorker() 
    : search_(new Search)
{
    loop_.start([this]() { search_->iterative_deepening(); });
}

SearchWorker::~SearchWorker() {
    stop();
    wait_for_completion();
}

void SearchWorker::set_silent(bool s) {
    search_->set_silent(s);
}

void SearchWorker::go(const Board &root, const SearchLimits &limits, 
        UCISearchConfig usc, const Stack *st, bool ponder) 
{
    stop();
    loop_.pause();
    loop_.wait_for_completion();

    search_->setup(root, limits, usc, st, ponder);

    loop_.resume();
}

void SearchWorker::stop() {
    search_->atomic_stop();
}

void SearchWorker::stop_pondering() {
    search_->stop_pondering();
}

void SearchWorker::wait_for_completion() {
    loop_.wait_for_completion();
}


