cmake_minimum_required(VERSION 3.10)
project(lamac C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g3 -O0)
endif()

add_executable(lamac
    src/main.cpp
    src/lexer.cpp
    src/ast.cpp
    src/parser.cpp
)

target_include_directories(lamac PRIVATE src)
