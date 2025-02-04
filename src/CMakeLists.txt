cmake_minimum_required(VERSION 3.16...3.22)
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

pkg_check_modules(PYTHON python3-embed)

if (NOT PYTHON_FOUND)
    pkg_check_modules(PYTHON python3)
endif ()

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
                    --nsversion=${UCA_ABI_VERSION}.0
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
#{{{ Installation
install(TARGETS uca
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT libraries)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libuca.pc
        DESTINATION ${CMAKE_INSTALL_PKGCONFIGDIR}
        COMPONENT libraries)

install(FILES ${uca_HDRS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/uca
        COMPONENT headers)

if(WITH_GIR)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${GIR_XML}
            DESTINATION ${CMAKE_INSTALL_GIRDIR}
            COMPONENT libraries)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${GIR_TYPELIB}
            DESTINATION ${CMAKE_INSTALL_TYPELIBDIR}
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
#{{{ Configure
find_program(INTROSPECTION_SCANNER "g-ir-scanner")
find_program(INTROSPECTION_COMPILER "g-ir-compiler")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libuca.pc.in"
               "${CMAKE_CURRENT_BINARY_DIR}/libuca.pc" @ONLY IMMEDIATE)
#}}}