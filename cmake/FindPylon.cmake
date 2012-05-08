
# Try to find libpco.so
#
# Defines
#
# PCO_FOUND - system has libpco
# PCO_INCLUDE_DIRS - libpco include directory
# PCO_LIBRARIES - pco library

find_package(YAT)

find_package(PackageHandleStandardArgs)

find_path(PYLON_DIRS include/pylon/PylonBase.h HINTS /opt/pylon ENV PYLON_ROOT)
set(GENICAM_ROOT ${PYLON_DIRS}/genicam)
set(PYLON_INCLUDE_DIRS ${PYLON_DIRS}/include ${GENICAM_ROOT}/library/CPP/include)

# check for 32/64 bit
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(PYLON_LIB_DIRS ${PYLON_DIRS}/lib64 ${PYLON_DIRS}/bin ${GENICAM_ROOT}/bin/Linux64_x64
     ${GENICAM_ROOT}/bin/Linux64_x64/GenApi/Generic)
else()
  set(PYLON_LIB_DIRS ${PYLON_DIRS}/lib64 ${PYLON_DIRS}/bin ${GENICAM_ROOT}/bin/Linux32_i86
     ${GENICAM_ROOT}/bin/Linux32_i86/GenApi/Generic)
endif()

find_library(PYLONBASE_LIB pylonbase PATHS ${PYLON_LIB_DIRS})
find_library(PYLONUTILITY_LIB pylonutility PATHS ${PYLON_LIB_DIRS})
find_library(PYLONGIGESUPP_LIB pylongigesupp PATHS ${PYLON_LIB_DIRS})
find_library(GENAPI_LIB GenApi_gcc40_v2_1 PATHS ${PYLON_LIB_DIRS})
find_library(GCBASE_LIB GCBase_gcc40_v2_1 PATHS ${PYLON_LIB_DIRS})
set (PYLON_LIBS ${PYLONBASE_LIB} 
  ${PYLONUTILITY_LIB}
  ${PYLONGIGESUPP_LIB}
  ${GENAPI_LIB}
  ${GCBASE_LIB}
  ${YAT_LIB})

find_package_handle_standard_args(PYLON DEFAULT_MSG PYLON_LIBS PYLON_INCLUDE_DIRS)

find_package(PkgConfig)
pkg_check_modules(BASLER_PYLON baslerpylon>=0.1.0 REQUIRED)
MESSAGE(LIBRARY_DIRS ${BASLER_PYLON_LIBRARY_DIRS})
find_library(BASLERPYLON_LIB baslerpylon PATHS ${BASLER_PYLON_LIBRARY_DIRS})

set (PYLON_LIBS ${PYLON_LIBS} ${BASLERPYLON_LIB})

