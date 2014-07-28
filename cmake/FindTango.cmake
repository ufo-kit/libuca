# Try to find libtango.so
#
# Defines
#
# TANGO_FOUND - system has libtango
# TANGO_INCLUDE_DIRS - libtango include directory
# TANGO_LIBRARIES - tango library

find_package(PackageHandleStandardArgs)

find_path(TANGO_INCLUDE_DIRS tango/tango.h)
find_library(TANGO_LIBRARIES tango)

find_package_handle_standard_args(TANGO DEFAULT_MSG TANGO_LIBRARIES TANGO_INCLUDE_DIRS)

if(TANGO_INCLUDE_DIRS)
   set(TANGO_INCLUDE_DIRS "${TANGO_INCLUDE_DIRS}/tango")
endif()

mark_as_advanced(
    TANGO_INCLUDE_DIRS
    TANGO_LIBRARIES
)
