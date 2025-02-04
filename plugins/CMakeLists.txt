set(UCA_CONFIGDIR "${CMAKE_CURRENT_BINARY_DIR}/../src")

add_subdirectory(mock)
add_subdirectory(file)

message(STATUS "Building the following plugins:")
set(_plugins "mock;file")

foreach(_target IN LISTS _plugins)
    if (TARGET "uca${_target}")
        message(STATUS "  ${_target}")
    endif()
endforeach()