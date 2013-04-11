cmake_minimum_required(VERSION 2.8)

add_definitions("--std=c99 -Wall")

# --- Find packages and libraries ---------------------------------------------
find_package(PkgConfig)

pkg_check_modules(GTK2 gtk+-2.0>=2.22)
pkg_check_modules(GTHREAD2 gthread-2.0)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/control.glade
               ${CMAKE_CURRENT_BINARY_DIR})

# --- Build targets -----------------------------------------------------------
include_directories(${CMAKE_CURRENT_BINARY_DIR})

if (GTK2_FOUND)
    include_directories(${GTK2_INCLUDE_DIRS})

    add_executable(control
        control.c
        egg-property-cell-renderer.c
        egg-property-tree-view.c
        egg-histogram-view.c)

    target_link_libraries(control
        m
        uca
        ringbuffer
        ${GTK2_LIBRARIES} 
        ${GTHREAD2_LIBRARIES})

    install(TARGETS control
        RUNTIME DESTINATION bin)

    install(FILES control.glade
        DESTINATION share/libuca)
endif()
