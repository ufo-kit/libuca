project(ucafile C)

set(UCA_CAMERA_NAME "file")
set(PLUGIN_VERSION ${UCA_VERSION_STRING})
set(PLUGIN_REVISION ${CPACK_PACKAGE_RELEASE})
set(PLUGIN_REQUIRES "libuca = ${UCA_VERSION_STRING}")
set(PLUGIN_SUMMARY "File plugin for libuca")

if (HAVE_LIBTIFF)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../package-plugin.sh.in
		${CMAKE_CURRENT_BINARY_DIR}/../../package-plugin-${UCA_CAMERA_NAME}.sh)

	add_library(ucafile SHARED
		uca-file-camera.c)

	target_link_libraries(ucafile
		PUBLIC uca ${LIBTIFF})

	install(TARGETS ucafile
		LIBRARY DESTINATION ${CMAKE_INSTALL_PLUGINDIR}
		RUNTIME DESTINATION ${CMAKE_INSTALL_PLUGINDIR}
		COMPONENT ${UCA_CAMERA_NAME})
 endif()
