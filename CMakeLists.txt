cmake_minimum_required(VERSION 2.8)
project(uca C)

set(TARNAME "libuca")
set(UCA_VERSION_MAJOR "0")
set(UCA_VERSION_MINOR "4")
set(UCA_VERSION_PATCH "0")

set(UCA_VERSION_STRING "${UCA_VERSION_MAJOR}.${UCA_VERSION_MINOR}.${UCA_VERSION_PATCH}")
set(UCA_API_VERSION "0.1.0")
set(UCA_ABI_VERSION "0.1.0")
set(UCA_ABI_MAJOR_VERSION "0")

set(PACKAGE_VERSION "${UCA_VERSION_STRING}")
set(PACKAGE_NAME "${TARNAME}")
set(PACKAGE_TARNAME "${TARNAME}")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "http://ufo.kit.edu/ufo/newticket")

add_subdirectory(src)
add_subdirectory(doc)
add_subdirectory(test)
