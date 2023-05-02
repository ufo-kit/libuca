#{{{ Variables
set(libs uca)
#}}}
#{{{ Configure

list(APPEND libs ${LIBTIFF})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)
#}}}
#{{{ Targets
include_directories(${CMAKE_CURRENT_BINARY_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR})

set(BINARIES "benchmark" "gen-doc" "info")

foreach (BINARY ${BINARIES})
    add_executable(uca-${BINARY} ${BINARY}.c common.c)
    target_link_libraries(uca-${BINARY} PUBLIC ${libs})
endforeach ()

add_executable(uca-grab grab.c common.c)
target_link_libraries(uca-grab PUBLIC ${libs})

if (GNU)
  target_link_libraries(uca-grab PUBLIC m)
endif()


install(TARGETS uca-benchmark uca-grab uca-gen-doc uca-info
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT executables)
#}}}
