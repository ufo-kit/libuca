cmake_minimum_required(VERSION 2.8)

# --- Set sources -------------------------------------------------------------
set(uca_SRCS
    uca-camera.c
    )

set(uca_HDRS
    uca-camera.h
    )


# --- Find packages and libraries ---------------------------------------------
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# --- Find camera interfaces
find_package(PCO)
find_package(PF)
find_package(IPE)

# --- Find frame grabber interfaces
find_package(FgLib5)
find_package(ClSerMe4)

# --- Miscellanous packages
find_package(PkgConfig)

pkg_check_modules(GLIB2 glib-2.0>=2.24 REQUIRED)
pkg_check_modules(GOBJECT2 gobject-2.0>=2.24 REQUIRED)

set(uca_LIBS
    ${GLIB2_LIBRARIES} 
    ${GOBJECT2_LIBRARIES})

# --- Build options -----------------------------------------------------------
option(HAVE_MOCK_CAMERA "Camera: Dummy" OFF)


# --- Add sources if camera/framegrabber access sources are available ---------
if (PF_FOUND)
    option(HAVE_PHOTON_FOCUS "Camera: Photon Focus MV2-D1280-640-CL-8" ON)

    if (HAVE_PHOTON_FOCUS)
        set(uca_SRCS ${uca_SRCS} cameras/uca-pf-camera.c)
        set(uca_HDRS ${uca_HDRS} cameras/uca-pf-camera.h)
        set(uca_LIBS ${uca_LIBS} ${PF_LIBRARIES})

        include_directories(${PF_INCLUDE_DIRS})
    endif()
endif()

if (PCO_FOUND AND CLSERME4_FOUND AND FGLIB5_FOUND)
    option(HAVE_PCO_CL "Camera: CameraLink-based pco" ON)

    if (HAVE_PCO_CL)
        set(uca_SRCS ${uca_SRCS} cameras/uca-pco-camera.c)
        set(uca_HDRS ${uca_HDRS} cameras/uca-pco-camera.h)

        set(uca_LIBS ${uca_LIBS}
            ${PCO_LIBRARIES}
            ${CLSERME4_LIBRARY}
            ${FGLIB5_LIBRARY})

        include_directories(
            ${PCO_INCLUDE_DIRS}
            ${CLSERME4_INCLUDE_DIR}
            ${FGLIB5_INCLUDE_DIR})
    endif()
endif()

if (IPE_FOUND)
    option(HAVE_IPE_CAMERA "Camera: Custom IPE based on Xilinx FPGA" ON)

    if (HAVE_IPE_CAMERA)
        set(uca_SRCS ${uca_SRCS} cameras/uca-ipe-camera.c)
        set(uca_HDRS ${uca_HDRS} cameras/uca-ipe-camera.h)
        set(uca_LIBS ${uca_LIBS} ${IPE_LIBRARIES})

        include_directories(${IPE_INCLUDE_DIRS})
    endif()
endif()

if (HAVE_MOCK_CAMERA)
    set(uca_SRCS ${uca_SRCS} cameras/uca-mock-camera.c)
    set(uca_HDRS ${uca_HDRS} cameras/uca-mock-camera.h)
endif()


# --- Configure step
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/cameras
    ${GLIB2_INCLUDE_DIRS}
    ${GOBJECT2_INCLUDE_DIRS})

# --- Build target ------------------------------------------------------------
add_definitions("-std=c99 -Wall")

add_library(uca-gobject SHARED ${uca_SRCS})

set_target_properties(uca-gobject PROPERTIES
      VERSION ${UCA_ABI_VERSION}
      SOVERSION ${UCA_VERSION_MINOR})

target_link_libraries(uca-gobject 
    ${uca_LIBS})


# --- Install target ----------------------------------------------------------
set(LIB_INSTALL_DIR "lib${LIB_SUFFIX}")

install(TARGETS uca-gobject
    LIBRARY DESTINATION ${LIB_INSTALL_DIR})

install(FILES ${uca_HDRS}
    DESTINATION include/uca)

# --- install pkg-config file
set(prefix      ${CMAKE_INSTALL_PREFIX})
set(exec_prefix "\${prefix}")
set(libdir      "${LIB_INSTALL_DIR}")
set(includedir  "\${prefix}/include")
set(VERSION     ${UCA_VERSION_STRING})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/uca.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/uca.pc" @ONLY IMMEDIATE)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/uca.pc DESTINATION lib/pkgconfig)


set(CPACK_PACKAGE_DESCRIPTION "Unified Camera Access library")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Abstract interface for different camera classes and frame grabber devices")
set(CPACK_PACKAGE_NAME "libuca")

# --- Distro specific
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.3.6), libgcc1 (>= 1:4.1)")
set(CPACK_SET_DESTDIR ON)

set(CPACK_PACKAGE_CONTACT "Matthias Vogelgesang")
set(CPACK_PACKAGE_VENDOR "Karlsruhe Institute of Technology/IPE")
set(CPACK_PACKAGE_VERSION_MAJOR ${UCA_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${UCA_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${UCA_VERSION_PATCH})
set(VERSION ${UCA_VERSION_STRING})

set(CPACK_GENERATOR "DEB;RPM;")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES "tags" ".bzr" ".swp")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "libuca-${UCA_VERSION_STRING}" CACHE INTERNAL "tarball basename")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${UCA_VERSION_STRING}-${CMAKE_SYSTEM_PROCESSOR}")

include(CPack)
