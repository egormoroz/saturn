#ifndef UCI_HPP
#define UCI_HPP

#include <variant>
#include <iosfwd>
#include "board/board.hpp"
#include "search_stack.hpp"

#define sync_cout std::cout << IO_LOCK
#define sync_endl std::endl << IO_UNLOCK

enum SyncCout {
    IO_LOCK,
    IO_UNLOCK
};

std::ostream& operator<<(std::ostream&, SyncCout sc);

class Engine;

namespace UCI {

void main_loop(Engine &eng);

} //namespace UCI

#endif
