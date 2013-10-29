cmake_minimum_required(VERSION 2.6)
project(ucapf C)

find_package(PF)
find_package(FgLib5)
find_package(ClSerMe4)

if (PF_FOUND AND CLSERME4_FOUND AND FGLIB5_FOUND)
    set(UCA_CAMERA_NAME "pco")
    set(PLUGIN_VERSION "1.0.0")
    set(PLUGIN_REVISION "0")
    set(PLUGIN_REQUIRES "libuca >= 1.2.0")

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../package-plugin.sh.in
                   ${CMAKE_CURRENT_BINARY_DIR}/../../package-plugin-${UCA_CAMERA_NAME}.sh)

    include_directories(${PF_INCLUDE_DIRS}
                        ${CLSERME4_INCLUDE_DIR}
                        ${FGLIB5_INCLUDE_DIR})

    add_library(ucapf SHARED
                uca-pf-camera.c)

    target_link_libraries(ucapf
                          ${UCA_DEPS}
                          ${PF_LIBRARIES}
                          ${CLSERME4_LIBRARY}
                          ${FGLIB5_LIBRARY})

    install(TARGETS ucapf
            LIBRARY DESTINATION ${LIB_INSTALL_DIR}/uca
            COMPONENT ${UCA_CAMERA_NAME})
endif()
