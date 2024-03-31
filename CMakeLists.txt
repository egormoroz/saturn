cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 17)

project(saturn)

add_executable(saturn 
    main.cpp zobrist.cpp perft.cpp tt.cpp selfplay.cpp pack.cpp book.cpp
    parameters.cpp board/board.cpp board/board_moves.cpp board/parse.cpp
    board/validate.cpp board/see.cpp movgen/attack.cpp movgen/generate.cpp
    primitives/utility.cpp searchstack.cpp movepicker.cpp uci.cpp
    search/searchworker.cpp search/search.cpp nnue/evaluate.cpp)

if (DEFINED ENV{EVALFILE})
    message(STATUS "$ENV{EVALFILE}")
    add_compile_definitions(EVALFILE="$ENV{EVALFILE}")
endif()


if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /GL /LTCG")
else()
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(saturn PRIVATE Threads::Threads)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -march=native -mtune=native -flto=auto")
endif()

