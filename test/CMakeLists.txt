cmake_minimum_required(VERSION 2.8)

add_definitions("--std=c99 -Wall")

# --- Find packages and libraries ---------------------------------------------
find_package(PkgConfig)

pkg_check_modules(GTK2 gtk+-2.0>=2.22)
pkg_check_modules(GTHREAD2 gthread-2.0)
pkg_check_modules(GLIB2 glib-2.0>=2.24 REQUIRED)
pkg_check_modules(GOBJECT2 gobject-2.0>=2.24 REQUIRED)

#include_directories(${CMAKE_SOURCE_DIR}/src)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/control.glade ${CMAKE_CURRENT_BINARY_DIR})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/run.py ${CMAKE_CURRENT_BINARY_DIR})

# --- Build targets -----------------------------------------------------------
include_directories(
    ${GLIB2_INCLUDE_DIRS}
    ${GOBJECT2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}/../src/
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

message("${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(grab grab.c)
add_executable(grab-async grab-async.c)
add_executable(benchmark benchmark.c)

target_link_libraries(benchmark uca ${GLIB2_LIBRARIES} ${GOBJECT2_LIBRARIES})
target_link_libraries(grab uca ${GLIB2_LIBRARIES} ${GOBJECT2_LIBRARIES})
target_link_libraries(grab-async uca ${GLIB2_LIBRARIES} ${GOBJECT2_LIBRARIES})

if (GTK2_FOUND)
    include_directories(${GTK2_INCLUDE_DIRS})

    add_executable(control 
        control.c 
        egg-property-cell-renderer.c
        egg-property-tree-view.c)

    target_link_libraries(control uca
        ${GTK2_LIBRARIES} ${GTHREAD2_LIBRARIES})

    install(TARGETS control
        RUNTIME DESTINATION bin)

    install(FILES control.glade
        DESTINATION share/libuca)
endif()

if (HAVE_MOCK_CAMERA)
    add_executable(test-mock test-mock.c)
    target_link_libraries(test-mock uca
        ${GLIB2_LIBRARIES} ${GOBJECT2_LIBRARIES})
endif()

add_executable(test-all test-all.c)
target_link_libraries(test-all uca
    ${GLIB2_LIBRARIES} ${GOBJECT2_LIBRARIES})
