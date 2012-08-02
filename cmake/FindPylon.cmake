
# Try to find libpco.so
#
# Defines
#
# PYLON_FOUND - system has libpco
# PYLON_INCLUDE_DIR - libpco include directory
# PYLON_LIB - pco library

# check for environment variable PYLON_ROOT
find_package(YAT)
find_package(PkgConfig)
find_package(PackageHandleStandardArgs)

if (NOT "$ENV{PYLON_ROOT}" STREQUAL "")
  message("PYLON_ROOT=$ENV{PYLON_ROOT}")

  find_library(YAT_LIB yat ${YAT_LIBRARY_DIRS})

  pkg_check_modules(LIBPYLONCAM pyloncam>=0.1 REQUIRED)
  if (DEFINED LIBPYLONCAM_OTHER_PREFIX)
    string(REPLACE ${LIBPYLONCAM_PREFIX} ${LIBPYLONCAM_OTHER_PREFIX}
      LIBPYLONCAM_INCLUDEDIR ${LIBPYLONCAM_INCLUDEDIR})
    string(REPLACE ${LIBPYLONCAM_PREFIX} ${LIBPYLONCAM_OTHER_PREFIX}
      LIBPYLONCAM_LIBRARIES ${LIBPYLONCAM_LIBRARIES})
    string(REPLACE ${LIBPYLONCAM_PREFIX} ${LIBPYLONCAM_OTHER_PREFIX}
      LIBPYLONCAM_LIBDIR ${LIBPYLONCAM_LIBDIR})
  endif()

  find_package_handle_standard_args(PYLON DEFAULT_MSG LIBPYLONCAM_INCLUDEDIR LIBPYLONCAM_LIBRARIES)

  mark_as_advanced(
    LIBPYLONCAM_INCLUDEDIR
    LIBPYLONCAM_LIBRARIES
    LIBPYLONCAM_LIBDIR)

else()
  message("Environment variable PYLON_ROOT not found! => unable to build pylon camera support")
endif()
