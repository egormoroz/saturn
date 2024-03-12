cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 17)

project(saturn)

add_executable(saturn 
    main.cpp zobrist.cpp perft.cpp tt.cpp
    board/board.cpp board/board_moves.cpp board/parse.cpp 
    board/validate.cpp board/see.cpp movgen/attack.cpp 
    movgen/magic.cpp movgen/generate.cpp primitives/utility.cpp
    core/eval.cpp tree.cpp searchstack.cpp movepicker.cpp
    cli.cpp core/searchworker.cpp nnue/evaluate.cpp
    core/search.cpp selfplay.cpp pack.cpp)


include(CheckIPOSupported)
check_ipo_supported(RESULT LTO_SUPPORTED)
if(LTO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO OFF)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)
endif()

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
else()
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(saturn PRIVATE Threads::Threads)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -march=native -mtune=native")
endif()

