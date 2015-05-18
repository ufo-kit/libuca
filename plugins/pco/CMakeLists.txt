cmake_minimum_required(VERSION 2.6)
project(ucapco C)

find_package(PCO)
find_package(FgLib5)
find_package(ClSerSis)

if (PCO_FOUND)
    set(UCA_CAMERA_NAME "pco")
    set(PLUGIN_VERSION "1.2.1")
    set(PLUGIN_REVISION "1")
    set(PLUGIN_REQUIRES "libuca >= 2.0.0")
    set(PLUGIN_SUMMARY "libpco plugin for libuca")

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../package-plugin.sh.in
                   ${CMAKE_CURRENT_BINARY_DIR}/../../package-plugin-${UCA_CAMERA_NAME}.sh)

    include_directories(${PCO_INCLUDE_DIRS}
                        ${clsersis_INCLUDE_DIR}
                        ${FgLib5_INCLUDE_DIR}
                        ${CMAKE_CURRENT_SOURCE_DIR}
                        ${CMAKE_CURRENT_BINARY_DIR}
                        ${UCA_CONFIGDIR})

    create_enums(uca-pco-enums
                 ${CMAKE_CURRENT_SOURCE_DIR}/../../src/uca-enums
                 uca-pco-camera.h)

    add_library(ucapco SHARED
                uca-pco-camera.c
                uca-pco-enums.c)

    target_link_libraries(ucapco
                          ${UCA_DEPS}
                          ${PCO_LIBRARIES}
                          ${clsersis_LIBRARY}
                          ${FgLib5_LIBRARY})

    install(TARGETS ucapco
            LIBRARY DESTINATION ${UCA_PLUGINDIR}
            COMPONENT ${UCA_CAMERA_NAME})
endif()
