cmake_minimum_required(VERSION 2.8)

add_definitions("--std=c99 -Wall")

# --- Find packages and libraries ---------------------------------------------
find_package(TIFF)

# --- Build targets -----------------------------------------------------------
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    )

set(libs uca)

if (TIFF_FOUND)
    set(HAVE_LIBTIFF "1")
    list(APPEND libs ${TIFF_LIBRARIES})
    include_directories(${TIFF_INCLUDE_DIRS})
endif ()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# --- gen-doc
add_executable(uca-gen-doc gen-doc.c)
target_link_libraries(uca-gen-doc ${libs})

# --- uca-grab
add_executable(uca-grab
               grab.c common.c)
target_link_libraries(uca-grab ringbuffer ${libs})

# --- uca-benchmark
add_executable(uca-benchmark
               benchmark.c common.c)
target_link_libraries(uca-benchmark ${libs})

install(TARGETS uca-benchmark uca-grab uca-gen-doc
        RUNTIME DESTINATION bin)
