cmake_minimum_required(VERSION 3.16)

project(uca C)

#{{{ CMake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
#}}}
#{{{ Metadata
set(UCA_DESCRIPTION "Unified Camera Access")

# These are software release versions
set(UCA_VERSION_MAJOR 2)
set(UCA_VERSION_MINOR 4)
set(UCA_VERSION_PATCH 0)
set(UCA_VERSION_STRING "${UCA_VERSION_MAJOR}.${UCA_VERSION_MINOR}.${UCA_VERSION_PATCH}")

# Increase the ABI version when binary compatibility cannot be guaranteed, e.g.
# symbols have been removed, function signatures, structures, constants etc.
# changed.
set(UCA_ABI_VERSION 2)
#}}}
#{{{ Macros
# create_enums
# @prefix: prefix of the output enum files
# @template_prefix: prefix of the template (should be src/uca-enums)
# @header_list: list with header files that should be searched for enums
function(create_enums prefix template_prefix header_list)
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
endfunction()
#}}}
#{{{ Configure
include(PkgConfigVars)
include(GNUInstallDirs)

set(CMAKE_INSTALL_GIRDIR        "${CMAKE_INSTALL_DATAROOTDIR}/gir-1.0")
set(CMAKE_INSTALL_PLUGINDIR     "${CMAKE_INSTALL_LIBDIR}/uca")
set(CMAKE_INSTALL_TYPELIBDIR    "${CMAKE_INSTALL_LIBDIR}/girepository-1.0")
set(CMAKE_INSTALL_PKGCONFIGDIR  "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/package.sh.in
               ${CMAKE_CURRENT_BINARY_DIR}/package.sh)
#}}}
#{{{ Common dependencies
find_package(PkgConfig REQUIRED)
find_program(GLIB2_MKENUMS glib-mkenums REQUIRED)
pkg_check_modules(GLIB2 REQUIRED glib-2.0>=2.38)
pkg_check_modules(GOBJECT2 REQUIRED gobject-2.0>=2.38)
pkg_check_modules(GMODULE2 REQUIRED gmodule-2.0>=2.38)
pkg_check_modules(GIO2 REQUIRED gio-2.0>=2.38)

set(GLIB_VERSION_MIN_REQUIRED "GLIB_VERSION_2_38")
set(GLIB_VERSION_MAX_ALLOWED "GLIB_VERSION_2_38")

link_directories(${GLIB2_LIBRARY_DIRS})
#}}}
#{{{ Common includes
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GLIB2_INCLUDE_DIRS}
    ${GOBJECT2_INCLUDE_DIRS}
    ${GMODULE2_INCLUDE_DIRS}
    ${GIO2_INCLUDE_DIRS})
#}}}
#{{{ Common variables
set(UCA_DEPS
    ${GLIB2_LIBRARIES}
    ${GOBJECT2_LIBRARIES}
    ${GMODULE2_LIBRARIES}
    ${GIO2_LIBRARIES})

set(UCA_ENUM_HDRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/uca-camera.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/pco/uca-pco-camera.h)
#}}}
#{{{ Common definitions
add_compile_options("-std=c99" "-Wall" "-fPIC")
#}}}
#{{{ Subdirectories
add_subdirectory(src)
add_subdirectory(plugins)
add_subdirectory(test)
add_subdirectory(bin)
#}}}