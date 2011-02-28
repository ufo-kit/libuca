cmake_minimum_required(VERSION 2.8)

add_definitions("--std=c99 -Wall")

include_directories(${CMAKE_SOURCE_DIR}/src)

add_executable(enum enum.c)
add_executable(grab grab.c)

target_link_libraries(enum uca)
target_link_libraries(grab uca)
