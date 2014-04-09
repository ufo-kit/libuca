cmake_minimum_required(VERSION 2.6)
project(ucapylon C)
set(VERSION "1.1.0")

find_package(Pylon)

if (PYLON_FOUND)
    set(UCA_CAMERA_NAME "pylon")
    set(PLUGIN_REVISION "1")
    set(PLUGIN_VERSION ${VERSION})
    set(PLUGIN_SUMMARY "Pylon plugin for libuca")
    set(PLUGIN_CHANGELOG "${CMAKE_CURRENT_SOURCE_DIR}/changelog.txt")
    set(PLUGIN_DESCRIPTION "Plugin for the Basler GigE CCD Camera.")
    set(PLUGIN_REQUIRES "libuca >= 1.3.0, libpyloncam >= 0.2.3")
    set(PLUGIN_VENDOR "ANKA Computing Group")

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../package-plugin.sh.in
                   ${CMAKE_CURRENT_BINARY_DIR}/../../package-plugin-${UCA_CAMERA_NAME}.sh)

    include_directories(${LIBPYLONCAM_INCLUDEDIR}
                        ${CMAKE_CURRENT_SOURCE_DIR}
                        ${CMAKE_CURRENT_BINARY_DIR}
                        ${UCA_CONFIGDIR})

    create_enums(uca-pylon-enums
                 ${CMAKE_CURRENT_SOURCE_DIR}/../../src/uca-enums
                 uca-pylon-camera.h)

    add_library(ucapylon SHARED
                uca-pylon-camera.c
                uca-pylon-enums.c)

    target_link_libraries(ucapylon
                          ${UCA_DEPS}
                          ${LIBPYLONCAM_LIBRARIES})

    install(TARGETS ucapylon
            LIBRARY DESTINATION ${UCA_PLUGINDIR}
            COMPONENT ${UCA_CAMERA_NAME})
endif()
