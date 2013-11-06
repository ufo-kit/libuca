cmake_minimum_required(VERSION 2.6)
project(ucapylon C)

find_package(Pylon)

if (PYLON_FOUND)
    set(UCA_CAMERA_NAME "pylon")

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
                uca-pylon-enums.c
                uca-pylon-camera.c)

    target_link_libraries(ucapylon
                          ${UCA_DEPS}
                          ${LIBPYLONCAM_LIBRARIES})

    install(TARGETS ucapylon
            LIBRARY DESTINATION ${UCA_PLUGINDIR}
            COMPONENT ${UCA_CAMERA_NAME})
endif()
