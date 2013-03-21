cmake_minimum_required(VERSION 2.8)
project(uca C)

# --- Set sources -------------------------------------------------------------
set(uca_SRCS
    uca-camera.c
    uca-plugin-manager.c
    )

set(uca_HDRS
    uca-camera.h
    uca-plugin-manager.h)

# --- Generate enum file
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/uca-enums.h
    COMMAND ${GLIB2_MKENUMS}
    ARGS
        --template uca-enums.h.template
        ${UCA_ENUM_HDRS} > ${CMAKE_CURRENT_BINARY_DIR}/uca-enums.h
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${UCA_ENUM_HDRS}
            ${CMAKE_CURRENT_SOURCE_DIR}/uca-enums.h.template)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/uca-enums.c
    COMMAND ${GLIB2_MKENUMS}
    ARGS
        --template uca-enums.c.template
        ${UCA_ENUM_HDRS} > ${CMAKE_CURRENT_BINARY_DIR}/uca-enums.c
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${UCA_ENUM_HDRS}
            ${CMAKE_CURRENT_BINARY_DIR}/uca-enums.h
            ${CMAKE_CURRENT_SOURCE_DIR}/uca-enums.c.template
    )


# --- Configure ---------------------------------------------------------------

find_program(INTROSPECTION_SCANNER "g-ir-scanner")
find_program(INTROSPECTION_COMPILER "g-ir-compiler")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(prefix      ${CMAKE_INSTALL_PREFIX})
if (CI_INSTALL_PREFIX)
    set(prefix  ${CI_INSTALL_PREFIX})
endif()

set(exec_prefix "\${prefix}")
set(libdir      ${prefix}/${LIB_INSTALL_DIR})
set(includedir  "\${prefix}/include")
set(VERSION     ${UCA_VERSION_STRING})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libuca.pc.in"
               "${CMAKE_CURRENT_BINARY_DIR}/libuca.pc" @ONLY IMMEDIATE)


# --- Build target ------------------------------------------------------------

add_library(uca SHARED
    ${uca_SRCS}
    ${CMAKE_CURRENT_BINARY_DIR}/uca-enums.c)

set_target_properties(uca PROPERTIES
      VERSION ${UCA_ABI_VERSION}
      SOVERSION ${UCA_VERSION_MAJOR})

target_link_libraries(uca ${UCA_DEPS})


# --- Build introspection files -----------------------------------------------

if (INTROSPECTION_SCANNER AND INTROSPECTION_COMPILER)
    option(WITH_GIR "Build introspection files" ON)

    if (WITH_GIR)
        set(GIR_PREFIX "Uca-${UCA_ABI_VERSION}")
        set(GIR_XML "${GIR_PREFIX}.gir")
        set(GIR_TYPELIB "${GIR_PREFIX}.typelib")
        set(_gir_input)

        foreach(_src ${uca_SRCS} ${uca_HDRS})
            list(APPEND _gir_input "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
        endforeach()

        list(APPEND _gir_input "${CMAKE_CURRENT_BINARY_DIR}/uca-enums.h")
        list(APPEND _gir_input "${CMAKE_CURRENT_BINARY_DIR}/uca-enums.c")

        add_custom_command(OUTPUT ${GIR_XML}
            COMMAND ${INTROSPECTION_SCANNER}
                    --namespace=Uca
                    --nsversion=${UCA_ABI_VERSION}
                    --library=uca
                    --no-libtool
                    --include=GObject-2.0
                    --include=GModule-2.0
                    --output ${GIR_XML}
                    --warn-all
                    ${_gir_input}
            DEPENDS ${uca_SRCS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

        add_custom_command(OUTPUT ${GIR_TYPELIB}
            COMMAND ${INTROSPECTION_COMPILER}
                    -o ${GIR_TYPELIB}
                    ${GIR_XML}
            DEPENDS ${GIR_XML}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

        add_custom_target(gir ALL DEPENDS ${GIR_XML} ${GIR_TYPELIB})
        add_dependencies(gir uca)

    endif()
endif()


# --- Build documentation -----------------------------------------------------

pkg_check_modules(GTK_DOC gtk-doc)

if(GTK_DOC_FOUND)
    option(WITH_GTK_DOC "Build documentation" ON)

    if (WITH_GTK_DOC)
        set(docs_dir "${CMAKE_CURRENT_BINARY_DIR}")
        set(docs_out "${docs_dir}/reference")
        file(MAKE_DIRECTORY ${docs_out})

        set(reference_files
            "${docs_out}/index.html"
            "${docs_out}/api-index-full.html"
            "${docs_out}/ch01.html"
            "${docs_out}/UcaCamera.html"
            "${docs_out}/UcaPluginManager.html"
            "${docs_out}/style.css"
            "${docs_out}/uca.devhelp2"
            "${docs_out}/home.png"
            "${docs_out}/left.png"
            "${docs_out}/right.png"
            "${docs_out}/up.png")

        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/uca-docs.xml.in
            ${docs_out}/uca-docs.xml)

        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/uca.types.in
            ${docs_out}/uca.types)

        # Create scangobj.sh from scangobj.sh.in
        get_directory_property(_current_include_dirs INCLUDE_DIRECTORIES)
        set(GTK_DOC_CFLAGS)
        foreach(_incl ${_current_include_dirs})
            set(GTK_DOC_CFLAGS "-I${_incl} ${GTK_DOC_CFLAGS}")
        endforeach()

        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/scangobj.sh.in" "${docs_out}/scangobj.sh")

        find_program(GTK_DOC_SCAN gtkdoc-scan REQUIRED)
        find_program(GTK_DOC_SCANGOBJ gtkdoc-scangobj REQUIRED)
        find_program(GTK_DOC_MKDB gtkdoc-mkdb REQUIRED)
        find_program(GTK_DOC_MKHTML gtkdoc-mkhtml REQUIRED)

        add_custom_command(OUTPUT ${docs_out}/uca-decl.txt
            COMMAND ${GTK_DOC_SCAN}
                    --module=uca
                    --source-dir=${CMAKE_CURRENT_SOURCE_DIR}
                    DEPENDS uca
            WORKING_DIRECTORY ${docs_out})

        add_custom_command(OUTPUT ${docs_out}/uca.args
            COMMAND sh scangobj.sh
            DEPENDS uca ${docs_out}/uca-decl.txt
            WORKING_DIRECTORY ${docs_out})

        add_custom_command(OUTPUT ${docs_out}/sgml.stamp
            COMMAND ${GTK_DOC_MKDB}
                    --module=uca
                    --source-dir=${CMAKE_CURRENT_SOURCE_DIR}
                    --output-format=xml
                    DEPENDS ${docs_out}/uca.args
            WORKING_DIRECTORY ${docs_out})

        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/html.stamp
            COMMAND ${GTK_DOC_MKHTML}
                    uca
                    ${docs_out}/uca-docs.xml
            DEPENDS ${docs_out}/sgml.stamp ${docs_out}/uca-docs.xml
            WORKING_DIRECTORY ${docs_out})

        add_custom_target(reference ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/html.stamp)

        install(FILES ${reference_files} DESTINATION share/gtk-doc/html/uca)
    endif()
endif()


# --- Install target ----------------------------------------------------------

install(TARGETS uca
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        COMPONENT libraries)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libuca.pc
        DESTINATION ${LIB_INSTALL_DIR}/pkgconfig
        COMPONENT libraries)

install(FILES ${uca_HDRS}
        DESTINATION include/uca
        COMPONENT headers)

if(WITH_GIR)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${GIR_XML}
            DESTINATION share/gir-1.0
            COMPONENT libraries)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${GIR_TYPELIB}
            DESTINATION ${LIB_INSTALL_DIR}/girepository-1.0
            COMPONENT libraries)
endif()


# --- Generate package description --------------------------------------------

set(CPACK_PACKAGE_DESCRIPTION "Unified Camera Access library")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GObject-based library for accessing scientific cameras")
set(CPACK_PACKAGE_NAME "libuca")

# this doesn't work when building RPMs on Jenkins
set(CPACK_SET_DESTDIR ON)

set(CPACK_PACKAGE_CONTACT "Matthias Vogelgesang")
set(CPACK_PACKAGE_VENDOR "Karlsruhe Institute of Technology/IPE")
set(CPACK_PACKAGE_VERSION_MAJOR ${UCA_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${UCA_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${UCA_VERSION_PATCH})
set(VERSION ${UCA_VERSION_STRING})

set(CPACK_GENERATOR "RPM;")

set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES ".git" "tags" ".bzr" ".swp")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "libuca-${UCA_VERSION_STRING}" CACHE INTERNAL "tarball basename")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${UCA_VERSION_STRING}-${CMAKE_SYSTEM_PROCESSOR}")


include(CPack)
