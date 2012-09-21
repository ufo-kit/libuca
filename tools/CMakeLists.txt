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

add_executable(gen-doc gen-doc.c)

target_link_libraries(gen-doc uca 
    ${GLIB2_LIBRARIES}
    ${GOBJECT2_LIBRARIES}
    )
