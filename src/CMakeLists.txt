cmake_minimum_required(VERSION 2.8)

# --- Set sources -------------------------------------------------------------
set(uca_SRCS
    uca.c
    uca-cam.c
    )

set(uca_HDRS
    uca.h
    uca-cam.h
    )

set(uca_LIBS "")

# --- Find packages and libraries ---------------------------------------------
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# --- Find camera interfaces
find_package(PCO)

# --- Find frame grabber interfaces
find_package(FgLib5)
find_package(ClSerMe4)

# --- Add sources if camera/framegrabber access sources are available ---------
if(PCO_FOUND)
    set(HAVE_PCO_EDGE TRUE)

    set(uca_SRCS 
        ${uca_SRCS}
        cameras/pco.c
        )

    set(uca_LIBS 
        ${uca_LIBS}
        ${PCO_LIBRARIES}
        )

    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PCO_INCLUDE_DIRS}
        )
endif()

if(CLSERME4_FOUND AND FGLIB5_FOUND)
    set(HAVE_ME4 TRUE)

    set(uca_SRCS 
        ${uca_SRCS}
        grabbers/me4.c
        )

    set(uca_LIBS 
        ${uca_LIBS}
        ${CLSERME4_LIBRARY}
        ${FGLIB5_LIBRARY}
        )

    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CLSERME4_INCLUDE_DIR}
        ${FGLIB5_INCLUDE_DIR}
        )
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR})


# --- Build target ------------------------------------------------------------
add_library(uca SHARED ${uca_SRCS})

target_link_libraries(uca ${uca_LIBS})
