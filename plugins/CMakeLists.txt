set(UCA_CONFIGDIR "${CMAKE_CURRENT_BINARY_DIR}/../src")

add_subdirectory(mock)
add_subdirectory(file)
add_subdirectory(kiro)
add_subdirectory(pf)
add_subdirectory(pco)
add_subdirectory(pylon)
add_subdirectory(ufo)
add_subdirectory(dexela)

message("-- Building the following plugins:")
set(_plugins "mock;file;kiro;pf;pco;pylon;ufo;dexela")

foreach(_target ${_plugins})
    if (TARGET "uca${_target}")
        message("--  ${_target}")
    endif()
endforeach()
