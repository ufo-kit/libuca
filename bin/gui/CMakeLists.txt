cmake_minimum_required(VERSION 3.16...3.22)

find_package(PkgConfig REQUIRED)
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)

pkg_check_modules(GTK2 REQUIRED gtk+-2.0>=2.22)
pkg_check_modules(GTHREAD2 REQUIRED gthread-2.0)

if (GTK2_FOUND)
    set(GRESOURCES_XML "${CMAKE_CURRENT_SOURCE_DIR}/uca-camera-control.gresource.xml")
    set(GRESOURCES_H "${CMAKE_CURRENT_BINARY_DIR}/resources.h")
    set(GRESOURCES_C "${CMAKE_CURRENT_BINARY_DIR}/resources.c")

    add_custom_command(
        OUTPUT ${GRESOURCES_H}
        COMMAND ${GLIB_COMPILE_RESOURCES}
            --target ${GRESOURCES_H}
            --generate-header ${GRESOURCES_XML}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/uca-camera-control.glade" ${GRESOURCES_XML}
    )

    add_custom_command(
        OUTPUT ${GRESOURCES_C}
        COMMAND ${GLIB_COMPILE_RESOURCES}
            --target ${GRESOURCES_C}
            --generate-source ${GRESOURCES_XML}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/uca-camera-control.glade" ${GRESOURCES_XML}
    )

    include_directories(${GTK2_INCLUDE_DIRS}
                        ${CMAKE_CURRENT_BINARY_DIR})

    add_executable(uca-camera-control
        uca-camera-control.c
        egg-property-cell-renderer.c
        egg-property-tree-view.c
        egg-histogram-view.c
        ${GRESOURCES_C}
    )

    add_custom_target(resources DEPENDS ${GRESOURCES_C} ${GRESOURCES_H})
    add_dependencies(uca-camera-control resources)

    target_link_libraries(uca-camera-control
        m
        uca
        ${GTK2_LIBRARIES}
        ${GTHREAD2_LIBRARIES})

    install(TARGETS uca-camera-control
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT executables)
endif()