cmake_minimum_required(VERSION 2.6)

#{{{ Variables
set(libs uca)
#}}}
#{{{ Configure
find_package(TIFF)

if (TIFF_FOUND)
    set(HAVE_LIBTIFF "1")
    list(APPEND libs ${TIFF_LIBRARIES})
    include_directories(${TIFF_INCLUDE_DIRS})
endif ()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)
#}}}
#{{{ Targets
include_directories(${CMAKE_CURRENT_BINARY_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR})

set(BINARIES "benchmark" "gen-doc" "info")

foreach (BINARY ${BINARIES})
    add_executable(uca-${BINARY} ${BINARY}.c common.c)
    target_link_libraries(uca-${BINARY} ${libs})
endforeach ()

add_executable(uca-grab grab.c common.c)
target_link_libraries(uca-grab ${libs} m)

install(TARGETS uca-benchmark uca-grab uca-gen-doc uca-info
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT executables)
#}}}
