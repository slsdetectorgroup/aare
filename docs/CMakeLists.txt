# SPDX-License-Identifier: MPL-2.0
find_package(Doxygen REQUIRED)
find_package(Sphinx REQUIRED)

#Doxygen
set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

#Sphinx
set(SPHINX_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(SPHINX_BUILD ${CMAKE_CURRENT_BINARY_DIR})


file(GLOB_RECURSE SPHINX_SOURCE_FILES
     CONFIGURE_DEPENDS
     RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/src"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.rst"
)

foreach(relpath IN LISTS SPHINX_SOURCE_FILES)
    set(src  "${CMAKE_CURRENT_SOURCE_DIR}/src/${relpath}")
    set(dst  "${SPHINX_BUILD}/src/${relpath}")

    message(STATUS "Copying ${src} to ${dst}")
    configure_file("${src}" "${dst}" COPYONLY)
endforeach()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/conf.py.in"
    "${SPHINX_BUILD}/conf.py"
    @ONLY
)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/figures"
     DESTINATION "${SPHINX_BUILD}") 

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/static/extra.css"
    "${SPHINX_BUILD}/static/css/extra.css"
    @ONLY
)

add_custom_target(
    docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
    COMMAND ${SPHINX_EXECUTABLE} -a -b html
    -Dbreathe_projects.aare=${CMAKE_CURRENT_BINARY_DIR}/xml
    -c "${SPHINX_BUILD}"
    ${SPHINX_BUILD}/src 
    ${SPHINX_BUILD}/html
    COMMENT "Generating documentation with Sphinx"
)

