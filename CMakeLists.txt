cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 17)

project(gm_bit)
add_executable(gm_bit 
    main.cpp zobrist.cpp perft.cpp uci.cpp tt.cpp movepicker.cpp
    board/board.cpp board/board_moves.cpp board/load_fen.cpp 
    board/validate.cpp board/ok_capture.cpp movgen/attack.cpp 
    movgen/magic.cpp movgen/generate.cpp primitives/utility.cpp
    core/search.cpp core/eval.cpp core/search_impl.cpp
    core/quiesce.cpp core/engine.cpp core/defer.cpp
    tree.cpp)

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
else()
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(gm_bit PRIVATE Threads::Threads)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -march=native")
endif()

