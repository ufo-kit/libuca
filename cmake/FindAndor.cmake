# Try to find libatcore.so (probably needs to be checked on completeness)
#
# Defines
#
# ANDOR_FOUND - system has libatcore
# ANDOR_INCLUDE_DIRS - libatcore include directory
# ANDOR_LIBRARIES - libatcore library

find_package(PackageHandleStandardArgs)

find_path(ANDOR_INCLUDE_DIRS atcore.h)
find_library(ANDOR_LIBRARIES atcore)

find_package_handle_standard_args(ANDOR DEFAULT_MSG ANDOR_LIBRARIES ANDOR_INCLUDE_DIRS)

mark_as_advanced(
	ANDOR_INCLUDE_DIRS
	ANDOR_LIBRARIES
)
