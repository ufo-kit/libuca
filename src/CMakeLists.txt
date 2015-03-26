cmake_minimum_required(VERSION 2.6)
project(uca C)

#{{{ Sources
set(uca_SRCS
    uca-camera.c
    uca-plugin-manager.c
    uca-ring-buffer.c
    )

set(uca_HDRS
    uca-camera.h
    uca-plugin-manager.h
    uca-ring-buffer.h
    )

create_enums(uca-enums
             ${CMAKE_CURRENT_SOURCE_DIR}/uca-enums
             ${uca_HDRS})
#}}}
#{{{ Variables
if (CI_INSTALL_PREFIX)
    set(UCA_PREFIX "${CI_INSTALL_PREFIX}")
endif()
#}}}
#{{{ Configure
find_program(INTROSPECTION_SCANNER "g-ir-scanner")
find_program(INTROSPECTION_COMPILER "g-ir-compiler")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libuca.pc.in"
               "${CMAKE_CURRENT_BINARY_DIR}/libuca.pc" @ONLY IMMEDIATE)
#}}}
#{{{ Target
include_directories(${CMAKE_CURRENT_BINARY_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR})

add_library(uca SHARED
    ${uca_SRCS}
    ${CMAKE_CURRENT_BINARY_DIR}/uca-enums.c)

set_target_properties(uca PROPERTIES
      VERSION ${UCA_VERSION_STRING}
      SOVERSION ${UCA_ABI_VERSION})

target_link_libraries(uca ${UCA_DEPS})
#}}}
#{{{ Python

pkg_check_modules(PYTHON python)

if (PYTHON_FOUND)
    option(WITH_PYTHON_MULTITHREADING "Enable Python multithreading support" ON)

    if (WITH_PYTHON_MULTITHREADING)
        include_directories(${PYTHON_INCLUDE_DIRS})
        target_link_libraries(uca ${PYTHON_LIBRARIES})
    endif ()
endif ()

#}}}
#{{{ GObject introspection
pkg_check_modules(GOBJECT_INTROSPECTION gobject-introspection-1.0)

if (GOBJECT_INTROSPECTION_FOUND)
    option(WITH_GIR "Build introspection files" ON)

    if (WITH_GIR)
        pkg_check_variable(gobject-introspection-1.0 g_ir_scanner)
        pkg_check_variable(gobject-introspection-1.0 g_ir_compiler)

        set(GIR_PREFIX "Uca-${UCA_ABI_VERSION}.0")
        set(GIR_XML "${GIR_PREFIX}.gir")
        set(GIR_TYPELIB "${GIR_PREFIX}.typelib")
        set(_gir_input)

        foreach(_src ${uca_SRCS} ${uca_HDRS})
            list(APPEND _gir_input "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
        endforeach()

        list(APPEND _gir_input "${CMAKE_CURRENT_BINARY_DIR}/uca-enums.h")
        list(APPEND _gir_input "${CMAKE_CURRENT_BINARY_DIR}/uca-enums.c")

        add_custom_command(OUTPUT ${GIR_XML}
            COMMAND ${GOBJECT_INTROSPECTION_1.0_G_IR_SCANNER}
                    --namespace=Uca
                    --nsversion=${UCA_ABI_VERSION}
                    --library=uca
                    --no-libtool
                    --include=GObject-2.0
                    --include=GModule-2.0
                    --output ${GIR_XML}
                    --warn-all
                    --quiet
                    ${_gir_input}
            DEPENDS ${uca_SRCS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

        add_custom_command(OUTPUT ${GIR_TYPELIB}
            COMMAND ${GOBJECT_INTROSPECTION_1.0_G_IR_COMPILER}
                    -o ${GIR_TYPELIB}
                    ${GIR_XML}
            DEPENDS ${GIR_XML}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

        add_custom_target(gir ALL DEPENDS ${GIR_XML} ${GIR_TYPELIB})
        add_dependencies(gir uca)

    endif()
endif()
#}}}
#{{{ Documentation
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

        install(FILES ${reference_files}
                DESTINATION ${UCA_DATADIR}/gtk-doc/html/uca)
    endif()
endif()
#}}}
#{{{ Installation
install(TARGETS uca
        LIBRARY DESTINATION ${UCA_LIBDIR}
        COMPONENT libraries)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libuca.pc
        DESTINATION ${UCA_PKGCONFIGDIR}
        COMPONENT libraries)

install(FILES ${uca_HDRS}
        DESTINATION ${UCA_INCLUDEDIR}/uca
        COMPONENT headers)

if(WITH_GIR)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${GIR_XML}
            DESTINATION ${UCA_GIRDIR}
            COMPONENT libraries)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${GIR_TYPELIB}
            DESTINATION ${UCA_TYPELIBDIR}
            COMPONENT libraries)
endif()
#}}}
#{{{ CPack
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
#}}}
