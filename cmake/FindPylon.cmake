
# Try to find libpco.so
#
# Defines
#
# PYLON_FOUND - system has libpco
# PYLON_INCLUDE_DIR - libpco include directory
# PYLON_LIB - pco library

# check for environment variable PYLON_ROOT
message("DEFINED PYLON ROOT $ENV{PYLON_ROOT}")
if (NOT "$ENV{PYLON_ROOT}" STREQUAL "")
  message("PYLON_ROOT=$ENV{PYLON_ROOT}")
  set(ENV{LD_LIBRARY_PATH} "$ENV{LD_LIBRARY_PATH}:$ENV{PYLON_ROOT}/lib")
  set(ENV{LD_LIBRARY_PATH} "$ENV{LD_LIBRARY_PATH};$ENV{PYLON_ROOT}/lib64")
  set(ENV{LD_LIBRARY_PATH} "$ENV{LD_LIBRARY_PATH};$ENV{PYLON_ROOT}/genicam/bin/Linux64_x64")
  set(ENV{LD_LIBRARY_PATH} "$ENV{LD_LIBRARY_PATH};$ENV{PYLON_ROOT}/genicam/bin/Linux32_i86")


  find_package(YAT)
  find_package(PackageHandleStandardArgs)

  find_path(PYLON_INCLUDE_DIR libpyloncam/pylon_camera.h)
  find_library(PYLON_LIB pyloncam)

  message("INCLUDE ${PYLON_INCLUDE_DIR}")

  find_package_handle_standard_args(PYLON DEFAULT_MSG PYLON_LIB PYLON_INCLUDE_DIR)

else()
  message("Environment variable PYLON_ROOT not found! => unable to build pylon camera support")
endif()
