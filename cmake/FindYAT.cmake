
find_package(PkgConfig)
pkg_check_modules(YAT yat>=1.7.7 REQUIRED)

find_library(YAT_LIB yat ${YAT_LIBRARY_DIRS})

