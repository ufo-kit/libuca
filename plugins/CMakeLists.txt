set(UCA_CONFIGDIR "${CMAKE_CURRENT_BINARY_DIR}/../src")

add_subdirectory(mock)
add_subdirectory(pf)
add_subdirectory(pco)
add_subdirectory(pylon)
add_subdirectory(ufo)
add_subdirectory(dexela)

if (CMAKE_CXX_COMPILER)
    add_subdirectory(xkit)
endif()
