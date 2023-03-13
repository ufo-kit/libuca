configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gtester.xsl
               ${CMAKE_CURRENT_BINARY_DIR}/gtester.xsl)

add_executable(test-mock test-mock.c)
add_executable(test-ring-buffer test-ring-buffer.c)

target_link_libraries(test-mock PUBLIC uca)
target_link_libraries(test-ring-buffer PUBLIC uca)
