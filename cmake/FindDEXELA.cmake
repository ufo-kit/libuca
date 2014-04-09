# Try to find libdexela.so
#
# Defines
#
# DEXELA_FOUND - system has libdexela
# DEXELA_INCLUDE_DIRS - libdexela include directory
# DEXELA_LIBRARIES - dexela library

find_package(PackageHandleStandardArgs)

find_path(DEXELA_INCLUDE_DIRS dexela_api.h PATHS /usr/include /usr/include/dexela)
find_library(DEXELA_LIBRARIES dexela)

find_package_handle_standard_args(DEXELA DEFAULT_MSG DEXELA_LIBRARIES DEXELA_INCLUDE_DIRS)

mark_as_advanced(
    DEXELA_INCLUDE_DIRS
    DEXELA_LIBRARIES
)
