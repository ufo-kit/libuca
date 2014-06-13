cmake_minimum_required(VERSION 2.6)
project(ucakiro C)

find_package(KIRO)
find_package(Tango)

if(KIRO_FOUND AND TANGO_FOUND)
    set(UCA_CAMERA_NAME "kiro")
    set(PLUGIN_VERSION "0.1")
    set(PLUGIN_REVISION "1")
    set(PLUGIN_REQUIRES "libuca >= 1.2.0")
    set(PLUGIN_SUMMARY "KIRO plugin for libuca")

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../package-plugin.sh.in
                   ${CMAKE_CURRENT_BINARY_DIR}/../../package-plugin-${UCA_CAMERA_NAME}.sh)

    add_library(ucakiro SHARED
                uca-kiro-camera.c)

    include_directories(${KIRO_INCLUDE_DIRS})

    target_link_libraries(ucakiro
                          uca
                          ${UCA_DEPS}
                          ${KIRO_LIBRARIES}
                          ${TANGO_LIBRARIES})

    install(TARGETS ucakiro
            LIBRARY DESTINATION ${UCA_PLUGINDIR}
            COMPONENT ${UCA_CAMERA_NAME})
endif()
