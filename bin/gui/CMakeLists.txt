cmake_minimum_required(VERSION 2.6)

#{{{ Variables
set(CMAKE_INSTALL_GLADEDIR    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/libuca")
#}}}
#{{{ Configure
find_package(PkgConfig)

pkg_check_modules(GTK2 gtk+-2.0>=2.22)
pkg_check_modules(GTHREAD2 gthread-2.0)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/control.glade
               ${CMAKE_CURRENT_BINARY_DIR})
#}}}
#{{{ Target
if (GTK2_FOUND)
    include_directories(${GTK2_INCLUDE_DIRS}
                        ${CMAKE_CURRENT_BINARY_DIR})
    set(BINARY "uca-camera-control")

    add_executable(${BINARY}
        control.c
        egg-property-cell-renderer.c
        egg-property-tree-view.c
        egg-histogram-view.c)

    target_link_libraries(${BINARY}
        m
        uca
        ${GTK2_LIBRARIES}
        ${GTHREAD2_LIBRARIES})

    install(TARGETS ${BINARY}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT executables)

    install(FILES control.glade
            DESTINATION ${CMAKE_INSTALL_GLADEDIR}
            COMPONENT executables)
endif()
#}}}
