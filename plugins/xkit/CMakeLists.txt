cmake_minimum_required(VERSION 2.6)
project(ucaxkit CXX)

set(UCA_CAMERA_NAME "xkit")
set(PLUGIN_VERSION "0.1.0")
set(PLUGIN_REVISION "0")
set(PLUGIN_REQUIRES "libuca >= 1.2.0")

find_path (XKIT_INCLUDE_DIR
           NAMES dll_api.h mpxhw.h)

find_library (XKIT_LIBRARIES
              NAMES xKIT)

if (XKIT_INCLUDE_DIR AND XKIT_LIBRARIES)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../package-plugin.sh.in
                   ${CMAKE_CURRENT_BINARY_DIR}/../../package-plugin-${UCA_CAMERA_NAME}.sh)
    include_directories(${XKIT_INCLUDE_DIR})

    # We have to compile with g++ because the included header files have C++
    # style comments ...
    add_library(ucaxkit SHARED
                uca-xkit-camera.cc)

    target_link_libraries(ucaxkit
                          ${UCA_DEPS}
                          ${XKIT_LIBRARIES})

    install(TARGETS ucaxkit
            LIBRARY DESTINATION ${LIB_INSTALL_DIR}/uca
            COMPONENT ${UCA_CAMERA_NAME})
endif ()
