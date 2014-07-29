cmake_minimum_required(VERSION 2.6)
project(ucadexela C)
set(VERSION "1.2.0")

find_package(DEXELA)

if (DEXELA_FOUND)
    set(UCA_CAMERA_NAME "dexela")
    set(PLUGIN_REVISION "1")
    set(PLUGIN_VERSION ${VERSION})
    set(PLUGIN_SUMMARY "Dexela plugin for libuca")
    set(PLUGIN_CHANGELOG "${CMAKE_CURRENT_SOURCE_DIR}/changelog.txt")
    set(PLUGIN_DESCRIPTION "Plugin for the Dexela 1207 detector.")
    set(PLUGIN_REQUIRES "libuca >= 1.3.0, libdexela >= 1.1.0")
    set(PLUGIN_VENDOR "ANKA Computing Group")

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../package-plugin.sh.in
                   ${CMAKE_CURRENT_BINARY_DIR}/../../package-plugin-${UCA_CAMERA_NAME}.sh)

    include_directories(${DEXELA_INCLUDE_DIRS}
                        ${UCA_CONFIGDIR})

    add_library(ucadexela SHARED uca-dexela-camera.c)

    target_link_libraries(ucadexela ${UCA_DEPS} ${DEXELA_LIBRARIES})

    install(TARGETS ucadexela
            LIBRARY DESTINATION ${UCA_PLUGINDIR}
            COMPONENT ${UCA_CAMERA_NAME})
endif()
