cmake_minimum_required(VERSION 2.8)

# --- Set sources -------------------------------------------------------------
set(uca_SRCS
    uca.c
    uca-cam.c
    uca-grabber.c
    )

set(uca_HDRS
    uca.h
    uca-cam.h
    uca-grabber.h
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

# FIXME: create some kind of libphoton and search for it
set(HAVE_PHOTON_FOCUS TRUE)
if(HAVE_PHOTON_FOCUS)
    set(uca_SRCS
        ${uca_SRCS}
        cameras/pf.c
        )
endif()

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

# --- Install target ----------------------------------------------------------
install(TARGETS uca 
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib)

install(FILES ${uca_HDRS} 
    DESTINATION include/libuca)

set(CPACK_PACKAGE_DESCRIPTION "Unified Camera Access library")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Abstract interface for different camera classes and frame grabber devices")
set(CPACK_PACKAGE_NAME "libuca")

# --- Distro specific
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.3.6), libgcc1 (>= 1:4.1)")
set(CPACK_SET_DESTDIR ON)

set(CPACK_PACKAGE_CONTACT "Matthias Vogelgesang")
set(CPACK_PACKAGE_VENDOR "Karlsruhe Institute of Technology/IPE")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

set(CPACK_GENERATOR "DEB;RPM;")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}-${CMAKE_SYSTEM_PROCESSOR}")

include(CPack)
