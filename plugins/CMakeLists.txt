set(UCA_CONFIGDIR "${CMAKE_CURRENT_BINARY_DIR}/../src")

add_subdirectory(mock)
add_subdirectory(file)

message("-- Building the following plugins:")
set(_plugins "mock;file")

foreach(_target ${_plugins})
    if (TARGET "uca${_target}")
        message("--  ${_target}")
    endif()
endforeach()
