# Try to find libdexela.so
#
# Defines
#
# DEXELA_FOUND - system has libdexela
# DEXELA_INCLUDE_DIRS - libdexela include directory
# DEXELA_LIBRARIES - dexela library

find_package(PackageHandleStandardArgs)

find_path(DEXELA_INCLUDE_DIRS dexela_api.h PATHS /root/Dexela/include)
find_library(DEXELA_LIBRARIES dexela PATHS /root/Dexela/dexela-spike)

find_package_handle_standard_args(DEXELA DEFAULT_MSG DEXELA_LIBRARIES DEXELA_INCLUDE_DIRS)

mark_as_advanced(
    DEXELA_INCLUDE_DIRS
    DEXELA_LIBRARIES
)
