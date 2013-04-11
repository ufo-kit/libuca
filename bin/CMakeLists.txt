cmake_minimum_required(VERSION 2.8)

add_library(ringbuffer
            ${CMAKE_CURRENT_SOURCE_DIR}/common/ring-buffer.c)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)

add_subdirectory(gui)
add_subdirectory(tools)

