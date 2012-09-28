cmake_minimum_required(VERSION 2.8)
project(ucamock)

set(UCA_CAMERA_NAME "mock")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../package-plugin.sh.in
               ${CMAKE_CURRENT_BINARY_DIR}/../../package-plugin-${UCA_CAMERA_NAME}.sh)

add_library(ucamock SHARED
            uca-mock-camera.c)

target_link_libraries(ucamock
                      uca
                      ${UCA_DEPS})

install(TARGETS ucamock
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}/uca
        COMPONENT ${UCA_CAMERA_NAME})
