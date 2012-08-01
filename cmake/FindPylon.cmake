
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

if (NOT "$ENV{PYLON_ROOT}" STREQUAL "")
  message("PYLON_ROOT=$ENV{PYLON_ROOT}")
  set(ENV{LD_LIBRARY_PATH} "$ENV{LD_LIBRARY_PATH}:$ENV{PYLON_ROOT}/lib")
  set(ENV{LD_LIBRARY_PATH} "$ENV{LD_LIBRARY_PATH};$ENV{PYLON_ROOT}/lib64")
  set(ENV{LD_LIBRARY_PATH} "$ENV{LD_LIBRARY_PATH};$ENV{PYLON_ROOT}/genicam/bin/Linux64_x64")
  set(ENV{LD_LIBRARY_PATH} "$ENV{LD_LIBRARY_PATH};$ENV{PYLON_ROOT}/genicam/bin/Linux32_i86")

  pkg_check_modules(LIBPYLONCAM pyloncam>=0.1 REQUIRED)

  find_library(YAT_LIB yat ${YAT_LIBRARY_DIRS})
  find_package(PackageHandleStandardArgs)

  find_package_handle_standard_args(PYLON DEFAULT_MSG LIBPYLONCAM_INCLUDEDIR LIBPYLONCAM_LIBRARIES)

  mark_as_advanced(
    LIBPYLONCAM_INCLUDEDIR
    LIBPYLONCAM_LIBRARIES)

else()
  message("Environment variable PYLON_ROOT not found! => unable to build pylon camera support")
endif()
