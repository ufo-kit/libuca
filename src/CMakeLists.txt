cmake_minimum_required(VERSION 2.8)

# --- Set sources -------------------------------------------------------------
set(uca_SRCS
    uca-camera.c
    )

set(uca_HDRS
    uca-camera.h)

set(cameras)

# --- Find packages and libraries ---------------------------------------------
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# --- Find camera interfaces
find_package(PCO)
find_package(PF)
find_package(IPE)
find_package(Pylon)

# --- Find frame grabber interfaces
find_package(FgLib5)
find_package(ClSerMe4)

# --- Miscellanous packages
find_package(PkgConfig)
find_program(GLIB2_MKENUMS glib-mkenums REQUIRED)
pkg_check_modules(GLIB2 glib-2.0>=2.24 REQUIRED)
pkg_check_modules(GOBJECT2 gobject-2.0>=2.24 REQUIRED)

set(uca_LIBS
    ${GLIB2_LIBRARIES} 
    ${GOBJECT2_LIBRARIES})

# --- Build options -----------------------------------------------------------
option(HAVE_MOCK_CAMERA "Camera: Dummy" ON)


# --- Add sources if camera/framegrabber access sources are available ---------
if (PF_FOUND)
    option(HAVE_PHOTON_FOCUS "Camera: Photon Focus MV2-D1280-640-CL-8" ON)

    if (HAVE_PHOTON_FOCUS AND CLSERME4_FOUND AND FGLIB5_FOUND)
        list(APPEND uca_SRCS cameras/uca-pf-camera.c)
        list(APPEND uca_HDRS cameras/uca-pf-camera.h)
        list(APPEND cameras "Pf")

        set(uca_LIBS ${uca_LIBS} 
            ${CLSERME4_LIBRARY} 
            ${FGLIB5_LIBRARY} 
            ${PF_LIBRARIES})

        include_directories(${PF_INCLUDE_DIRS}
            ${CLSERME4_INCLUDE_DIR}
            ${FGLIB5_INCLUDE_DIR})
    endif()
endif()

if (PCO_FOUND AND CLSERME4_FOUND AND FGLIB5_FOUND)
    option(HAVE_PCO_CL "Camera: CameraLink-based pco" ON)

    if (HAVE_PCO_CL)
        list(APPEND uca_SRCS cameras/uca-pco-camera.c)
        list(APPEND uca_HDRS cameras/uca-pco-camera.h)
        list(APPEND cameras "Pco")

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
    option(HAVE_UFO_CAMERA "Camera: Custom based on Xilinx FPGA" ON)

    if (HAVE_UFO_CAMERA)
        list(APPEND uca_SRCS cameras/uca-ufo-camera.c)
        list(APPEND uca_HDRS cameras/uca-ufo-camera.h)
        list(APPEND cameras "Ufo")

        set(uca_LIBS ${uca_LIBS} ${IPE_LIBRARIES})

        include_directories(${IPE_INCLUDE_DIRS})
    endif()
endif()

if (PYLON_FOUND)
  option(HAVE_PYLON_CAMERA "Camera: Pylon based (Basler)" ON)

  if (HAVE_PYLON_CAMERA)
        list(APPEND uca_SRCS cameras/uca-pylon-camera.c)
        list(APPEND uca_HDRS cameras/uca-pylon-camera.h)
        list(APPEND cameras "Pylon")
        set(uca_LIBS ${uca_LIBS} ${LIBPYLONCAM_LIBRARIES})

        include_directories(${LIBPYLONCAM_INCLUDEDIR})
        link_directories(${LIBPYLONCAM_LIBDIR})
  endif()

endif()

if (HAVE_MOCK_CAMERA)
    list(APPEND uca_SRCS cameras/uca-mock-camera.c)
    list(APPEND uca_HDRS cameras/uca-mock-camera.h)
    list(APPEND cameras "Mock")
endif()

# --- Generate enum file
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/uca-enums.h
    COMMAND ${GLIB2_MKENUMS}
    ARGS
        --template uca-enums.h.template
        ${uca_HDRS} > ${CMAKE_CURRENT_BINARY_DIR}/uca-enums.h
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${uca_HDRS} 
            ${CMAKE_CURRENT_SOURCE_DIR}/uca-enums.h.template)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/uca-enums.c
    COMMAND ${GLIB2_MKENUMS}
    ARGS
        --template uca-enums.c.template
        ${uca_HDRS} > ${CMAKE_CURRENT_BINARY_DIR}/uca-enums.c
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${uca_HDRS} 
            ${CMAKE_CURRENT_BINARY_DIR}/uca-enums.h
            ${CMAKE_CURRENT_SOURCE_DIR}/uca-enums.c.template
    )

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

add_library(uca SHARED 
    ${uca_SRCS} 
    ${CMAKE_CURRENT_BINARY_DIR}/uca-enums.c)

set_target_properties(uca PROPERTIES
      VERSION ${UCA_ABI_VERSION}
      SOVERSION ${UCA_VERSION_MAJOR})

target_link_libraries(uca
    ${uca_LIBS})

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
            "${docs_out}/style.css"
            "${docs_out}/uca.devhelp2"
            "${docs_out}/home.png"
            "${docs_out}/left.png"
            "${docs_out}/right.png"
            "${docs_out}/up.png")

        # Put in uca-docs.xml and uca.types all cameras that are built
        set(_xml_doc_input)
        set(_types_input)
        foreach (_cam ${cameras})
            # add camera to the installed documentation
            list(APPEND reference_files "${docs_out}/Uca${_cam}Camera.html")

            string(TOLOWER ${_cam} _cam)
            set(_xml_doc_input "${_xml_doc_input}\n<xi:include href=\"xml/uca-${_cam}-camera.xml\"/>")
            set(_types_input "${_types_input}\nuca_${_cam}_camera_get_type")
        endforeach()

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
set(LIB_INSTALL_DIR "lib${LIB_SUFFIX}")

install(TARGETS uca
    LIBRARY DESTINATION ${LIB_INSTALL_DIR})

install(FILES ${uca_HDRS} 
    DESTINATION include/uca)

# --- install pkg-config file
set(prefix      ${CMAKE_INSTALL_PREFIX})
if (CI_INSTALL_PREFIX)
    set(prefix  ${CI_INSTALL_PREFIX})
endif()

set(exec_prefix "\${prefix}")
set(libdir      ${prefix}/${LIB_INSTALL_DIR})
set(includedir  "\${prefix}/include")
set(VERSION     ${UCA_VERSION_STRING})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/uca.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/uca.pc" @ONLY IMMEDIATE)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/uca.pc DESTINATION lib/pkgconfig)


set(CPACK_PACKAGE_DESCRIPTION "Unified Camera Access library")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Abstract interface for different camera classes and frame grabber devices")
set(CPACK_PACKAGE_NAME "libuca")

# --- Distro specific
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.3.6), libgcc1 (>= 1:4.1)")

# this doesn't work when building RPMs on Jenkins
set(CPACK_SET_DESTDIR ON)

set(CPACK_PACKAGE_CONTACT "Matthias Vogelgesang")
set(CPACK_PACKAGE_VENDOR "Karlsruhe Institute of Technology/IPE")
set(CPACK_PACKAGE_VERSION_MAJOR ${UCA_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${UCA_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${UCA_VERSION_PATCH})
set(VERSION ${UCA_VERSION_STRING})

set(CPACK_GENERATOR "DEB;RPM;")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES ".git" "tags" ".bzr" ".swp")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "libuca-${UCA_VERSION_STRING}" CACHE INTERNAL "tarball basename")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${UCA_VERSION_STRING}-${CMAKE_SYSTEM_PROCESSOR}")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../libuca.spec.in"
               "${CMAKE_CURRENT_BINARY_DIR}/../libuca.spec" @ONLY IMMEDIATE)

include(CPack)
