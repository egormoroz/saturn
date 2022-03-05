#ifndef UCI_HPP
#define UCI_HPP

#include <variant>
#include <iosfwd>
#include "board/board.hpp"
#include "timer.hpp"

#define sync_cout std::cout << IO_LOCK
#define sync_endl std::endl << IO_UNLOCK

enum SyncCout {
    IO_LOCK,
    IO_UNLOCK
};

std::ostream& operator<<(std::ostream&, SyncCout sc);


namespace UCI {
namespace cmd {

struct Position { 
    Board board; 
};

struct Go {
    //in milliseconds
    int time_left[COLOR_NB]{};

    //in milliseconds
    int increment[COLOR_NB]{};

    int max_depth{}, max_nodes{}, move_time{};
    
    bool infinite{};

    TimePoint start{};
};

struct Stop {};
struct Quit {};

} //namespace cmd

using Command = std::variant<cmd::Position, cmd::Go,
      cmd::Stop, cmd::Quit>;

struct Listener {
    virtual void accept(const Command &cmd) = 0;
    virtual ~Listener() = default;
};

void main_loop(Listener &listener);

} //namespace UCI

#endif
