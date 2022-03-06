cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 17)

project(gm_bit)
add_executable(gm_bit main.cpp zobrist.cpp perft.cpp uci.cpp
    board/board.cpp board/board_moves.cpp board/load_fen.cpp 
    board/validate.cpp board/see.cpp movgen/attack.cpp 
    movgen/magic.cpp movgen/generate.cpp primitives/utility.cpp
    core/search.cpp core/eval.cpp tt.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

