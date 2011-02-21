# Try to find libpco.so
#
# Defines
#
# PCO_FOUND - system has libpco
# PCO_INCLUDE_DIRS - libpco include directory
# PCO_LIBRARIES - pco library

find_package(PackageHandleStandardArgs)

find_path(PCO_INCLUDE_DIRS libpco/libpco.h)
find_library(PCO_LIBRARIES pco)

find_package_handle_standard_args(PCO DEFAULT_MSG PCO_LIBRARIES PCO_INCLUDE_DIRS)

mark_as_advanced(
    PCO_INCLUDE_DIRS
    PCO_LIBRARIES
)
