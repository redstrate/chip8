cmake_minimum_required(VERSION 3.16)
project(chip8)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(SDL2 REQUIRED)

add_subdirectory(extern)

add_library(chip8-shared
    src/emu.hpp
    src/emu.cpp)
target_include_directories(chip8-shared PUBLIC src)
set_target_properties(chip8-shared PROPERTIES CXX_STANDARD 17)

add_executable(chip8
    src/main.cpp
    src/compiler.hpp
    src/compiler.cpp)
target_link_libraries(chip8 PRIVATE SDL2::Core chip8-shared imgui glad)
target_include_directories(chip8 PRIVATE src)
set_target_properties(chip8 PROPERTIES CXX_STANDARD 17)

add_executable(chip8-tests
    tests/test.cpp)
target_link_libraries(chip8-tests PRIVATE chip8-shared doctest)
set_target_properties(chip8-tests PROPERTIES CXX_STANDARD 17)
