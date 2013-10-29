cmake_minimum_required(VERSION 2.6)
project(ucadexela C)
set(VERSION "1.0.0")

find_package(DEXELA)

if (DEXELA_FOUND)
    set(UCA_CAMERA_NAME "dexela")
    set(PLUGIN_REVISION "4")
    set(PLUGIN_VERSION "1.0.0")
    set(PLUGIN_SUMMARY "Dexela plugin for libuca")
    set(PLUGIN_CHANGELOG "${CMAKE_CURRENT_SOURCE_DIR}/changelog.txt")
    set(PLUGIN_DESCRIPTION "Plugin for the Dexela 1207 detector.")
    set(PLUGIN_REQUIRES "libuca = 1.1.0, libdexela >= 0.9.2")
    set(PLUGIN_VENDOR "ANKA Computing Group")

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../package-plugin.sh.in
                   ${CMAKE_CURRENT_BINARY_DIR}/../../package-plugin-${UCA_CAMERA_NAME}.sh)

    include_directories(${DEXELA_INCLUDE_DIRS})

    add_library(ucadexela SHARED uca-dexela-camera.c)

    target_link_libraries(ucadexela ${UCA_DEPS}  ${DEXELA_LIBRARIES})

    install(TARGETS ucadexela
            LIBRARY DESTINATION ${LIB_INSTALL_DIR}/uca
            COMPONENT ${UCA_CAMERA_NAME})
endif()
