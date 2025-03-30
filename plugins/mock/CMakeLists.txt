cmake_minimum_required(VERSION 2.6)
project(ucamock C)

set(UCA_CAMERA_NAME "mock")
set(PLUGIN_VERSION ${UCA_VERSION_STRING})
set(PLUGIN_REVISION ${CPACK_PACKAGE_RELEASE})
set(PLUGIN_REQUIRES "libuca = ${UCA_VERSION_STRING}")
set(PLUGIN_SUMMARY "Mock plugin for libuca")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../package-plugin.sh.in
               ${CMAKE_CURRENT_BINARY_DIR}/../../package-plugin-${UCA_CAMERA_NAME}.sh)

add_library(ucamock SHARED
            uca-mock-camera.c)

target_link_libraries(ucamock uca ${UCA_DEPS})
if (NOT WIN32)
    target_link_libraries(ucamock m)
endif()

install(TARGETS ucamock
        LIBRARY DESTINATION ${CMAKE_INSTALL_PLUGINDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_PLUGINDIR}
        COMPONENT ${UCA_CAMERA_NAME})
