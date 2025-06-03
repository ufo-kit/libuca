cmake_minimum_required(VERSION 3.11)
project(uca C)

option(WITH_PYTHON_MULTITHREADING "Enable Python multithreading support" ON)
option(WITH_GIR "Build introspection files" ON)
option(WITH_TIFF "Support loading tiff files" ON)
option(WITH_GUI "Build the GUI tools" ON)
option(WITH_TOOLS "Build the CLI tools" ON)
option(USE_FIND_PACKAGE_FOR_GLIB "Use find_package instead of pkg-config to find GLib dependencies" OFF)

if (USE_FIND_PACKAGE_FOR_GLIB)
    message(STATUS "Using find_package for GLib")
    find_package(glib REQUIRED)
    set(UCA_DEPS glib::glib)
else()
    message(STATUS "Using pkg-config for GLib")
    find_package(PkgConfig)
    find_program(GLIB2_MKENUMS glib-mkenums REQUIRED)
    pkg_check_modules(GLIB2 glib-2.0>=2.38 REQUIRED)
    pkg_check_modules(GOBJECT2 gobject-2.0>=2.38 REQUIRED)
    pkg_check_modules(GMODULE2 gmodule-2.0>=2.38 REQUIRED)
    pkg_check_modules(GIO2 gio-2.0>=2.38 REQUIRED)

    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
    include(PkgConfigVars)
    
    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${GLIB2_INCLUDE_DIRS}
        ${GOBJECT2_INCLUDE_DIRS}
        ${GMODULE2_INCLUDE_DIRS}
        ${GIO2_INCLUDE_DIRS}
    )
    set(UCA_DEPS
        ${GLIB2_LIBRARIES}
        ${GOBJECT2_LIBRARIES}
        ${GMODULE2_LIBRARIES}
        ${GIO2_LIBRARIES}
        m # Link the math library libm, it is used by glib
    )
endif()

if (WITH_TIFF)
    find_package(TIFF REQUIRED)
    set(HAVE_LIBTIFF "1")
endif()

#{{{ Metadata
set(UCA_DESCRIPTION "Unified Camera Access")

# These are software release versions
set(UCA_VERSION_MAJOR "2")
set(UCA_VERSION_MINOR "4")
set(UCA_VERSION_PATCH "0")
set(UCA_VERSION_STRING "${UCA_VERSION_MAJOR}.${UCA_VERSION_MINOR}.${UCA_VERSION_PATCH}")

# Increase the ABI version when binary compatibility cannot be guaranteed, e.g.
# symbols have been removed, function signatures, structures, constants etc.
# changed.
set(UCA_ABI_VERSION "2")
#}}}
#{{{ Macros
# create_enums
# @prefix: prefix of the output enum files
# @template_prefix: prefix of the template (should be src/uca-enums)
# @header_list: list with header files that should be searched for enums
macro(create_enums prefix template_prefix header_list)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${prefix}.h
        COMMAND python ${GLIB2_MKENUMS}
        ARGS
            --template ${template_prefix}.h.template
            ${header_list} > ${CMAKE_CURRENT_BINARY_DIR}/${prefix}.h
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${header_list})

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${prefix}.c
        COMMAND python ${GLIB2_MKENUMS}
        ARGS
            --template ${template_prefix}.c.template
            ${header_list} > ${CMAKE_CURRENT_BINARY_DIR}/${prefix}.c
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${header_list}
                ${CMAKE_CURRENT_BINARY_DIR}/${prefix}.h
        )
endmacro()
#}}}
#{{{ Configure
include(GNUInstallDirs)

if (MSVC)
    # On windows, plugins go next to the applications
    set(CMAKE_INSTALL_PLUGINDIR ${CMAKE_INSTALL_BINDIR})
    set(UCA_PLUGINDIR ".")
else()
    # On *NIX systems, they are installed into the system
    set(CMAKE_INSTALL_PLUGINDIR ${CMAKE_INSTALL_LIBDIR}/uca)
    set(UCA_PLUGINDIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_PLUGINDIR})
endif()

set(CMAKE_INSTALL_GIRDIR        "${CMAKE_INSTALL_DATAROOTDIR}/gir-1.0")
set(CMAKE_INSTALL_TYPELIBDIR    "${CMAKE_INSTALL_LIBDIR}/girepository-1.0")
set(CMAKE_INSTALL_PKGCONFIGDIR  "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/package.sh.in
               ${CMAKE_CURRENT_BINARY_DIR}/package.sh)
#}}}
#{{{ Common dependencies
find_program(GLIB2_MKENUMS glib-mkenums REQUIRED)

set(GLIB_VERSION_MIN_REQUIRED "GLIB_VERSION_2_38")
set(GLIB_VERSION_MAX_ALLOWED "GLIB_VERSION_2_38")

link_directories(${GLIB2_LIBRARY_DIRS})
#}}}

#{{{ Common definitions
if (GNU)
  add_definitions("-std=c99 -Wall -fPIC")
endif()
#}}}
#{{{ Subdirectories
add_subdirectory(src)
add_subdirectory(plugins)
add_subdirectory(test)
add_subdirectory(bin)
#}}}
