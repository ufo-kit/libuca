cmake_minimum_required(VERSION 2.8)

add_definitions("--std=c99 -Wall")

# --- Find packages and libraries ---------------------------------------------
find_package(PkgConfig)

pkg_check_modules(GTK2 gtk+-2.0>=2.22)
pkg_check_modules(GTHREAD2 gthread-2.0)
pkg_check_modules(GLIB2 glib-2.0>=2.24 REQUIRED)
pkg_check_modules(GOBJECT2 gobject-2.0>=2.24 REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/control.glade ${CMAKE_CURRENT_BINARY_DIR})

# --- Build targets -----------------------------------------------------------
include_directories(
    ${GLIB2_INCLUDE_DIRS}
    ${GOBJECT2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    )

if (GTK2_FOUND)
    include_directories(${GTK2_INCLUDE_DIRS})

    add_executable(control
        control.c
        ../ring-buffer.c    # yes this sucks and should be fixed
        egg-property-cell-renderer.c
        egg-property-tree-view.c
        egg-histogram-view.c)

    target_link_libraries(control uca
        ${GTK2_LIBRARIES} 
        ${GTHREAD2_LIBRARIES}
        m)

    install(TARGETS control
        RUNTIME DESTINATION bin)

    install(FILES control.glade
        DESTINATION share/libuca)
endif()
