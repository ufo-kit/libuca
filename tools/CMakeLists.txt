cmake_minimum_required(VERSION 2.8)

add_definitions("--std=c99 -Wall")

# --- Find packages and libraries ---------------------------------------------
find_package(PkgConfig)

pkg_check_modules(GLIB2 glib-2.0>=2.24 REQUIRED)
pkg_check_modules(GOBJECT2 gobject-2.0>=2.24 REQUIRED)

# --- Build targets -----------------------------------------------------------
include_directories(
    ${GLIB2_INCLUDE_DIRS}
    ${GOBJECT2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}/../src/
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

set(libs uca ${GLIB2_LIBRARIES} ${GOBJECT2_LIBRARIES})

add_executable(gen-doc gen-doc.c)
target_link_libraries(gen-doc ${libs})

add_executable(grab grab.c)
target_link_libraries(grab ${libs})

add_executable(grab-async grab-async.c)
target_link_libraries(grab-async ${libs})

add_executable(benchmark benchmark.c)
target_link_libraries(benchmark ${libs})

add_executable(perf perf-overhead.c)
target_link_libraries(perf ${libs})

add_subdirectory(gui)
