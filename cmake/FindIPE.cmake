# Try to find user-space driver for the IPE camera
#
# Defines
#
# IPE_FOUND - system has libpcidriver
# IPE_INCLUDE_DIRS - libpci include directory
# IPE_LIBRARIES - pci library

find_package(PackageHandleStandardArgs)

find_path(IPE_INCLUDE_DIRS pcilib.h)
find_library(IPE_LIBRARIES pcidriver)

find_package_handle_standard_args(IPE DEFAULT_MSG IPE_LIBRARIES IPE_INCLUDE_DIRS)

mark_as_advanced(
    IPE_INCLUDE_DIRS
    IPE_LIBRARIES
)
