cmake_minimum_required(VERSION 2.8)

#find_package(Doxygen)

# --- Create Doxygen source documentation by typing `make doc`
#if(DOXYGEN_FOUND)
#    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
#    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/publish.sh.in ${CMAKE_CURRENT_BINARY_DIR}/publish.sh)
#
#    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/html/index.html
#        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
#        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
#        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../src/uca.h
#        COMMENT "Generating API documentation with Doxygen" VERBATIM
#        )
#    add_custom_target(docs ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/html/index.html)
#endif(DOXYGEN_FOUND)
