cmake_minimum_required(VERSION 2.8)
project(ucadexela C)
set(VERSION "1.0.0")

find_package(DEXELA)

if (DEXELA_FOUND)
    set(UCA_CAMERA_NAME "dexela")
    set(PLUGIN_REVISION "2")
    set(PLUGIN_VERSION "1.0.0")
    set(PLUGIN_CHANGELOG "${CMAKE_CURRENT_SOURCE_DIR}/changelog.txt")
    set(PLUGIN_REQUIRES "libuca = 1.1.0, libdexela >= 0.9.2")
    set(PLUGIN_SPECIFIC_FLAGS
 "-D CPACK_PACKAGE_NAME=\"uca-plugin-dexela\" -D CPACK_PACKAGE_VERSION=\"${PLUGIN_VERSION}\" -D CPACK_RPM_PACKAGE_RELEASE=\"${PLUGIN_REVISION}\" -D CPACK_RPM_CHANGELOG_FILE=${PLUGIN_CHANGELOG} -D CPACK_RPM_PACKAGE_REQUIRES=${PLUGIN_REQUIRES}")

    set(CPACK_PACKAGE_VERSION ${VERSION})
    set(CPACK_GENERATOR "RPM")
    set(CPACK_PACKAGE_NAME "uca-plugin-dexela")
    set(CPACK_PACKAGE_RELEASE 1)
    set(CPACK_PACKAGE_CONTACT "Mihael Koep")
    set(CPACK_PACKAGE_VENDOR "ANKA Computing Group")
    set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../package-plugin.sh.in
                   ${CMAKE_CURRENT_BINARY_DIR}/../../package-plugin-${UCA_CAMERA_NAME}.sh)

    include_directories(${DEXELA_INCLUDE_DIRS})

    add_library(ucadexela SHARED
                uca-dexela-camera.c)

    target_link_libraries(ucadexela
                          ${UCA_DEPS}
                          ${DEXELA_LIBRARIES})

    install(TARGETS ucadexela
            LIBRARY DESTINATION ${LIB_INSTALL_DIR}/uca
            COMPONENT ${UCA_CAMERA_NAME})
endif()
