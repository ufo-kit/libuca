cmake_minimum_required(VERSION 2.6)
project(ucaufo C)

find_package(IPE)

if (IPE_FOUND)
    set(UCA_CAMERA_NAME "ufo")
    set(PLUGIN_VERSION "1.0.1")
    set(PLUGIN_REVISION "0")
    set(PLUGIN_REQUIRES "libuca >= 1.2.0")

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../package-plugin.sh.in
                   ${CMAKE_CURRENT_BINARY_DIR}/../../package-plugin-${UCA_CAMERA_NAME}.sh)

    include_directories(${IPE_INCLUDE_DIRS})

    add_library(ucaufo SHARED
                uca-ufo-camera.c)

    target_link_libraries(ucaufo
                          ${UCA_DEPS}
                          ${IPE_LIBRARIES})

    install(TARGETS ucaufo
            LIBRARY DESTINATION ${LIB_INSTALL_DIR}/uca
            COMPONENT ${UCA_CAMERA_NAME})
endif()
