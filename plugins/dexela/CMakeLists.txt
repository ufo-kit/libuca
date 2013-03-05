cmake_minimum_required(VERSION 2.8)
project(ucadexela C)
set(VERSION "1.0.0")

find_package(DEXELA)

if (DEXELA_FOUND)
    set(UCA_CAMERA_NAME "dexela")
    set(PLUGIN_SPECIFIC_FLAGS "-D CMAKE_CURRENT_BINARY_DIR=`pwd` -D CPACK_RPM_USER_BINARY_SPECFILE=${CMAKE_CURRENT_BINARY_DIR}/ucadexela.spec")

    set(CPACK_PACKAGE_VERSION ${VERSION})
    set(CPACK_GENERATOR "RPM")
    set(CPACK_PACKAGE_NAME "uca-plugin-dexela")
    set(CPACK_PACKAGE_RELEASE 1)
    set(CPACK_PACKAGE_CONTACT "Mihael Koep")
    set(CPACK_PACKAGE_VENDOR "ANKA Computing Group")
    set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ucadexela.spec.in
                   ${CMAKE_CURRENT_BINARY_DIR}/ucadexela.spec)
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
