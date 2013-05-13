cmake_minimum_required(VERSION 2.8)
project(uca C)

set(TARNAME "libuca")
set(UCA_VERSION_MAJOR "1")
set(UCA_VERSION_MINOR "3")
set(UCA_VERSION_PATCH "0dev")
set(UCA_DESCRIPTION "Unified Camera Access")

set(UCA_VERSION_STRING "${UCA_VERSION_MAJOR}.${UCA_VERSION_MINOR}.${UCA_VERSION_PATCH}")
set(UCA_ABI_VERSION "${UCA_VERSION_MAJOR}.${UCA_VERSION_MINOR}")

set(CPACK_PACKAGE_VERSION "${UCA_VERSION_STRING}")
set(CPACK_PACKAGE_NAME "${TARNAME}")
set(PACKAGE_TARNAME "${TARNAME}")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "http://ufo.kit.edu/ufo/newticket")

set(CPACK_GENERATOR "DEB;RPM")
set(CPACK_PACKAGE_RELEASE 3)
set(CPACK_RPM_PACKAGE_RELEASE 3)
set(CPACK_DEBIAN_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Matthias Vogelgesang <matthias.vogelgesang@kit.edu>")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION ${UCA_DESCRIPTION})
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION_SUMMARY ${UCA_DESCRIPTION})
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")

set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")

set(UCA_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})

set(LIB_INSTALL_DIR "lib${LIB_SUFFIX}")

set(UCA_ENUM_HDRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/uca-camera.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/pco/uca-pco-camera.h)


# ---  Common configuration ---------------------------------------------------

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/package.sh.in
               ${CMAKE_CURRENT_BINARY_DIR}/package.sh)

# --- Common flags ------------------------------------------------------------

add_definitions("-std=c99 -Wall")


# --- Common macros

# create_enums
# @prefix: prefix of the output enum files
# @template_prefix: prefix of the template (should be src/uca-enums)
# @header_list: list with header files that should be searched for enums
macro(create_enums prefix template_prefix header_list)
    ${CMAKE_CURRENT_SOURCE_DIR}")
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${prefix}.h
        COMMAND ${GLIB2_MKENUMS}
        ARGS
            --template ${template_prefix}.h.template
            ${header_list} > ${CMAKE_CURRENT_BINARY_DIR}/${prefix}.h
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${header_list})

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${prefix}.c
        COMMAND ${GLIB2_MKENUMS}
        ARGS
            --template ${template_prefix}.c.template
            ${header_list} > ${CMAKE_CURRENT_BINARY_DIR}/${prefix}.c
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${header_list}
                ${CMAKE_CURRENT_BINARY_DIR}/${prefix}.h
        )
endmacro()


# --- Common libraries --------------------------------------------------------

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

find_package(PkgConfig)
find_program(GLIB2_MKENUMS glib-mkenums REQUIRED)
pkg_check_modules(GLIB2 glib-2.0>=2.24 REQUIRED)
pkg_check_modules(GOBJECT2 gobject-2.0>=2.24 REQUIRED)
pkg_check_modules(GMODULE2 gmodule-2.0>=2.24 REQUIRED)
pkg_check_modules(GIO2 gio-2.0>=2.24 REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GLIB2_INCLUDE_DIRS}
    ${GOBJECT2_INCLUDE_DIRS}
    ${GMODULE2_INCLUDE_DIRS}
    ${GIO2_INCLUDE_DIRS})

set(UCA_DEPS
    ${GLIB2_LIBRARIES}
    ${GOBJECT2_LIBRARIES}
    ${GMODULE2_LIBRARIES}
    ${GIO2_LIBRARIES})

add_subdirectory(src)
add_subdirectory(plugins)
add_subdirectory(test)
add_subdirectory(bin)

include(CPack)

