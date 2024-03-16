#ifndef SEARCHWORKER_HPP
#define SEARCHWORKER_HPP

#include <memory>
#include "routine.hpp"
#include "search.hpp"

class SearchWorker {
public:
    SearchWorker();

    void set_silent(bool s);

    void go(const Board &root, const SearchLimits &limits,
            UCISearchConfig usc, const Stack *st = nullptr);

    void stop();
    void wait_for_completion();

private:
    std::unique_ptr<Search> search_;
    Routine loop_;
};

#endif
