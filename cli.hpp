#ifndef CLI_HPP
#define CLI_HPP

#include <iostream>
#include "board/board.hpp"
#include "searchstack.hpp"
#include "core/searchworker.hpp"

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

    Book book_;
    bool book_loaded_ = false;

    UCISearchConfig cfg_;
};

int enter_cli(int argc, char **argv);

#endif
