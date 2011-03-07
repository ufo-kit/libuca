cmake_minimum_required(VERSION 2.8)

add_definitions("--std=c99 -Wall")

# --- Find packages and libraries ---------------------------------------------
find_package(PkgConfig)

pkg_check_modules(GTK2 gtk+-2.0)

include_directories(${CMAKE_SOURCE_DIR}/src)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/control.glade
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# --- Build targets -----------------------------------------------------------
add_executable(enum enum.c)
add_executable(grab grab.c)

target_link_libraries(enum uca)
target_link_libraries(grab uca)

if (GTK2_FOUND)
    include_directories(${GTK2_INCLUDE_DIRS})
    add_executable(control control.c)
    target_link_libraries(control
        uca
        ${GTK2_LIBRARIES}
        )
endif()

