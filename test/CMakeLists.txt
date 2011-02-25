cmake_minimum_required(VERSION 2.8)

add_definitions("--std=c99 -Wall")

include_directories(${CMAKE_SOURCE_DIR}/src)

add_executable(test test.c)

target_link_libraries(test uca)
