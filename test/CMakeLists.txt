cmake_minimum_required(VERSION 2.6)

include_directories(${ucadexela_SOURCE_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gtester.xsl
               ${CMAKE_CURRENT_BINARY_DIR}/gtester.xsl)

add_executable(test-mock test-mock.c)
add_executable(test-ring-buffer test-ring-buffer.c)
add_executable(test-software-roi test-software-roi.c ${ucadexela_SOURCE_DIR}/software-roi.c)

target_link_libraries(test-mock uca ${UCA_DEPS})
target_link_libraries(test-ring-buffer uca ${UCA_DEPS})
target_link_libraries(test-software-roi ${UCA_DEPS})
