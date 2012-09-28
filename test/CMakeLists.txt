cmake_minimum_required(VERSION 2.8)

add_executable(test-mock test-mock.c)

target_link_libraries(test-mock uca ${UCA_DEPS})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gtester.xsl
               ${CMAKE_CURRENT_BINARY_DIR}/gtester.xsl)
