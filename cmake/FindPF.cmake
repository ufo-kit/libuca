# Try to find libpco.so
#
# Defines
#
# PCO_FOUND - system has libpco
# PCO_INCLUDE_DIRS - libpco include directory
# PCO_LIBRARIES - pco library

find_package(PackageHandleStandardArgs)

find_path(PF_INCLUDE_DIRS libpf/pfcam.h)
find_library(PF_LIBRARIES NAMES comdll mv2_d1280_640)

find_package_handle_standard_args(PF DEFAULT_MSG PF_LIBRARIES PF_INCLUDE_DIRS)

mark_as_advanced(
    PF_INCLUDE_DIRS
    PF_LIBRARIES
)
