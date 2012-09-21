cmake_minimum_required(VERSION 2.8)

add_definitions("--std=c99 -Wall")

# --- Find packages and libraries ---------------------------------------------
find_package(PkgConfig)

pkg_check_modules(GTK2 gtk+-2.0>=2.22)
pkg_check_modules(GTHREAD2 gthread-2.0)
pkg_check_modules(GLIB2 glib-2.0>=2.24 REQUIRED)
pkg_check_modules(GOBJECT2 gobject-2.0>=2.24 REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/control.glade ${CMAKE_CURRENT_BINARY_DIR})

# --- Build targets -----------------------------------------------------------
include_directories(
    ${GLIB2_INCLUDE_DIRS}
    ${GOBJECT2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}/../src/
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

if (HAVE_PYLON_CAMERA)
    set(GENICAM_ROOT $ENV{PYLON_ROOT}/genicam)
    # check for 32/64 bit
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(PYLON_LIB_DIRS $ENV{PYLON_ROOT}/lib64 $ENV{PYLON_ROOT}/bin ${GENICAM_ROOT}/bin/Linux64_x64
            ${GENICAM_ROOT}/bin/Linux64_x64/GenApi/Generic)
    else()
        set(PYLON_LIB_DIRS $ENV{PYLON_ROOT}/lib64 $ENV{PYLON_ROOT}/bin ${GENICAM_ROOT}/bin/Linux32_i86
            ${GENICAM_ROOT}/bin/Linux32_i86/GenApi/Generic)
    endif()
    link_directories(${PYLON_LIB_DIRS} ${LIBPYLONCAM_LIBDIR})
endif()

add_executable(grab grab.c)
add_executable(grab-async grab-async.c)
add_executable(benchmark benchmark.c)

target_link_libraries(benchmark uca ${GLIB2_LIBRARIES} ${GOBJECT2_LIBRARIES})
target_link_libraries(grab uca ${GLIB2_LIBRARIES} ${GOBJECT2_LIBRARIES})
target_link_libraries(grab-async uca ${GLIB2_LIBRARIES} ${GOBJECT2_LIBRARIES})

if (GTK2_FOUND)
    include_directories(${GTK2_INCLUDE_DIRS})

    add_executable(control 
        control.c 
        egg-property-cell-renderer.c
        egg-property-tree-view.c)

    target_link_libraries(control uca
        ${GTK2_LIBRARIES} ${GTHREAD2_LIBRARIES})

    install(TARGETS control
        RUNTIME DESTINATION bin)

    install(FILES control.glade
        DESTINATION share/libuca)
endif()

if (HAVE_MOCK_CAMERA)
    add_executable(test-mock test-mock.c)

    target_link_libraries(test-mock
        uca
        ${GLIB2_LIBRARIES}
        ${GOBJECT2_LIBRARIES})

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gtester.xsl
        ${CMAKE_CURRENT_BINARY_DIR}/gtester.xsl)
endif()

#add_executable(test-all test-all.c)
#target_link_libraries(test-all uca
#    ${GLIB2_LIBRARIES} ${GOBJECT2_LIBRARIES})
