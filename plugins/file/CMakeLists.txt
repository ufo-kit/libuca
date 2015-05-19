cmake_minimum_required(VERSION 2.6)
project(ucafile C)

find_package(TIFF)

if (TIFF_FOUND)
    set(UCA_CAMERA_NAME "file")
    set(PLUGIN_VERSION "0.0.2")
    set(PLUGIN_REVISION "1")
    set(PLUGIN_REQUIRES "libuca >= 2.0.0")
    set(PLUGIN_SUMMARY "File plugin for libuca")

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../package-plugin.sh.in
                   ${CMAKE_CURRENT_BINARY_DIR}/../../package-plugin-${UCA_CAMERA_NAME}.sh)

    include_directories(${TIFF_INCLUDE_DIRS})

    add_library(ucafile SHARED
                uca-file-camera.c)

    target_link_libraries(ucafile uca ${UCA_DEPS} ${TIFF_LIBRARIES})

    install(TARGETS ucafile
            LIBRARY DESTINATION ${UCA_PLUGINDIR}
            COMPONENT ${UCA_CAMERA_NAME})
endif ()
