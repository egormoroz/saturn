#ifndef CLI_HPP
#define CLI_HPP

#include <variant>
#include <string>
#include <map>
#include <mutex>
#include <iostream>
#include <string_view>
#include "board/board.hpp"
#include "searchstack.hpp"
#include "core/searchworker.hpp"

struct CoutWrapper {
    CoutWrapper(std::mutex &mtx) 
        : lock_(mtx) {}

    template<typename T>
    CoutWrapper& operator<<(T && t) {
        std::cout << t;
        return *this;
    }

    ~CoutWrapper() {
        std::cout.flush();
    }

private:
    std::lock_guard<std::mutex> lock_;
};

inline CoutWrapper sync_cout() {
    static std::mutex mutex;
    return CoutWrapper(mutex);
}

struct UciSpin { int64_t min, max, value; };
using UciOption = std::variant<bool, UciSpin, std::string>;
std::ostream& operator<<(std::ostream &os, const UciOption &opt);

class UCIContext {
public:
    UCIContext();

    void enter_loop();

private:
    void parse_position(std::istream &is);
    void parse_go(std::istream &is);
    void parse_setopt(std::istream &is);

    void update_option(std::string_view name, 
            const UciOption &opt);

    void print_info();

    std::map<std::string, UciOption> options_;
    Board board_;
    Stack st_;
    SearchWorker search_;
};

int enter_cli(int argc, char **argv);

#endif
