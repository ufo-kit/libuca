cmake_minimum_required(VERSION 2.6)
project(ucaandor C)
set(VERSION "0.0.1")

find_package(Andor)

if (ANDOR_FOUND)
    set(UCA_CAMERA_NAME "andor")
    set(PLUGIN_VERSION ${VERSION})
    set(PLUGIN_REVISION "1")
    set(PLUGIN_REQUIRES "libuca >= 1.2.0")
    set(PLUGIN_SUMMARY "Andor plugin for libuca")

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../package-plugin.sh.in
	    ${CMAKE_CURRENT_BINARY_DIR}/../../package-plugin-${UCA_CAMERA_NAME}.sh)

    add_library(ucaandor SHARED
            uca-andor-camera.c)

    target_link_libraries(ucaandor
                      uca
			${ANDOR_LIBRARIES}
                      ${UCA_DEPS})

    install(TARGETS ucaandor
        LIBRARY DESTINATION ${UCA_PLUGINDIR}
        COMPONENT ${UCA_CAMERA_NAME})
endif()
