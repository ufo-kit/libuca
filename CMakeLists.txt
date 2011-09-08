cmake_minimum_required(VERSION 2.8)
project(uca C)

set(UCA_VERSION_MAJOR "0")
set(UCA_VERSION_MINOR "4")
set(UCA_VERSION_PATCH "0")

set(UCA_VERSION_STRING "${UCA_VERSION_MAJOR}.${UCA_VERSION_MINOR}.${UCA_VERSION_PATCH}")

add_subdirectory(src)
add_subdirectory(doc)
add_subdirectory(test)
