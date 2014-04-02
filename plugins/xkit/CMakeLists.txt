cmake_minimum_required(VERSION 2.6)
project(ucaxkit C)

set(UCA_CAMERA_NAME "xkit")
set(PLUGIN_VERSION "0.1.0")
set(PLUGIN_REVISION "1")
set(PLUGIN_REQUIRES "libuca >= 1.2.0")


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../package-plugin.sh.in
               ${CMAKE_CURRENT_BINARY_DIR}/../../package-plugin-${UCA_CAMERA_NAME}.sh)

include_directories(${UCA_CONFIGDIR})

add_library(ucaxkit SHARED uca-xkit-camera.c)

target_link_libraries(ucaxkit ${UCA_DEPS})

install(TARGETS ucaxkit
        LIBRARY DESTINATION ${UCA_PLUGINDIR}
        COMPONENT ${UCA_CAMERA_NAME})
