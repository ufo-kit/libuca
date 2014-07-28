cmake_minimum_required(VERSION 2.6)
project(ucakiro)

find_package(KIRO)
find_package(Tango)

message(STATUS "TANGO INCLUDES: " ${TANGO_INCLUDE_DIRS})

if(KIRO_FOUND AND TANGO_FOUND)
    set(UCA_CAMERA_NAME "kiro")
    set(PLUGIN_VERSION "0.1")
    set(PLUGIN_REVISION "1")
    set(PLUGIN_REQUIRES "libuca >= 1.2.0")
    set(PLUGIN_SUMMARY "KIRO plugin for libuca")

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../package-plugin.sh.in
                   ${CMAKE_CURRENT_BINARY_DIR}/../../package-plugin-${UCA_CAMERA_NAME}.sh)

    add_library(ucakiro SHARED
                uca-kiro-camera.cpp)

    find_path(KIRO_INCLUDE_DIRS kiro-rdma.h)
    find_path(TANGO_INCLUDE_DIRS tango.h)
    include_directories(${KIRO_INCLUDE_DIRS} ${TANGO_INCLUDE_DIRS})

    set_source_files_properties(uca-kiro-camera.cpp PROPERTIES COMPILE_FLAGS "-std=c++0x -D_USING_TANGO_8_OR_ABOVE_")

    target_link_libraries(ucakiro
                          uca
                          ${UCA_DEPS}
                          ${KIRO_LIBRARIES}
                          ${TANGO_LIBRARIES})

    install(TARGETS ucakiro
            LIBRARY DESTINATION ${UCA_PLUGINDIR}
            COMPONENT ${UCA_CAMERA_NAME})
endif()
