#include "searchworker.hpp"

SearchWorker::SearchWorker() 
    : search_(new Search)
{
    loop_.start([this]() { search_->iterative_deepening(); });
}

void SearchWorker::set_silent(bool s) {
    search_->set_silent(s);
}

void SearchWorker::go(const Board &root, const SearchLimits &limits, 
        UCISearchConfig usc, const Stack *st) 
{
    stop();
    loop_.pause();
    loop_.wait_for_completion();

    search_->setup(root, limits, usc, st);

    loop_.resume();
}

void SearchWorker::stop() {
    search_->atomic_stop();
}

void SearchWorker::wait_for_completion() {
    loop_.wait_for_completion();
}


