cmake_minimum_required(VERSION 2.8)

add_definitions("--std=c99 -Wall")

# --- Find packages and libraries ---------------------------------------------
find_package(PkgConfig)
find_package(TIFF)

pkg_check_modules(GLIB2 glib-2.0>=2.24 REQUIRED)
pkg_check_modules(GOBJECT2 gobject-2.0>=2.24 REQUIRED)

# --- Build targets -----------------------------------------------------------
include_directories(
    ${GLIB2_INCLUDE_DIRS}
    ${GOBJECT2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

set(libs uca ${GLIB2_LIBRARIES} ${GOBJECT2_LIBRARIES})

if (TIFF_FOUND)
    set(HAVE_LIBTIFF "1")
    list(APPEND libs ${TIFF_LIBRARIES})
    include_directories(${TIFF_INCLUDE_DIRS})
endif ()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

add_executable(gen-doc gen-doc.c)
target_link_libraries(gen-doc ${libs})

add_executable(grab grab.c ring-buffer.c)
target_link_libraries(grab ${libs})

add_executable(benchmark benchmark.c)
target_link_libraries(benchmark ${libs})
