#ifndef CLI_HPP
#define CLI_HPP

#include <iostream>
#include "board/board.hpp"
#include "searchstack.hpp"
#include "search/searchworker.hpp"

#include "book.hpp"

class UCIContext {
public:
    UCIContext();

    void enter_loop();

private:
    void parse_position(std::istream &is);
    void parse_go(std::istream &is);
    void parse_go_perft(std::istream &is);
    void parse_setopt(std::istream &is);

    void print_info();

    Board board_;
    Stack st_;
    SearchWorker search_;
    StateInfo si_;

    int multipv_ = 1;

    Book book_;
    bool book_loaded_ = false;
};

#endif
