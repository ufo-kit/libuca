cmake_minimum_required(VERSION 2.6)
project(ucamock C)

set(UCA_CAMERA_NAME "mock")
set(PLUGIN_VERSION "1.0.1")
set(PLUGIN_REVISION "1")
set(PLUGIN_REQUIRES "libuca >= 1.2.0")
set(PLUGIN_SUMMARY "Mock plugin for libuca")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../package-plugin.sh.in
               ${CMAKE_CURRENT_BINARY_DIR}/../../package-plugin-${UCA_CAMERA_NAME}.sh)

add_library(ucamock SHARED
            uca-mock-camera.c)

target_link_libraries(ucamock uca m ${UCA_DEPS})

install(TARGETS ucamock
        LIBRARY DESTINATION ${UCA_PLUGINDIR}
        COMPONENT ${UCA_CAMERA_NAME})
