# Try to find libpco.so
#
# Defines
#
# PF_FOUND - system has libpco
# PF_INCLUDE_DIRS - libpco include directory
# PF_LIBRARIES - pco library

find_package(PackageHandleStandardArgs)

find_path(PF_INCLUDE_DIRS libpf/pfcam.h)
find_library(PF_LIBRARY_COMDLL NAMES comdll)
find_library(PF_LIBRARY_MV2 NAMES mv2_d1280_640)
find_library(PF_LIBRARY_PFCAM NAMES pfcam)

set(PF_LIBRARIES ${PF_LIBRARY_MV2} ${PF_LIBRARY_PFCAM} ${PF_LIBRARY_COMDLL})

find_package_handle_standard_args(PF DEFAULT_MSG PF_LIBRARIES PF_INCLUDE_DIRS)

mark_as_advanced(
    PF_INCLUDE_DIRS
    PF_LIBRARIES
)
