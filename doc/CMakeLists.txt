cmake_minimum_required(VERSION 2.8)

find_package(Doxygen)

# --- Create Doxygen source documentation by typing `make doc`
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/publish.sh.in ${CMAKE_CURRENT_BINARY_DIR}/publish.sh)

    # add 'ALL after 'doc' to create documentation, whenever you type make
    add_custom_target(doc
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM
        )
endif(DOXYGEN_FOUND)
