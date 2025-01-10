find_package(Doxygen REQUIRED)
find_package(Sphinx REQUIRED)

#Doxygen
set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

#Sphinx
set(SPHINX_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(SPHINX_BUILD ${CMAKE_CURRENT_BINARY_DIR})


file(GLOB SPHINX_SOURCE_FILES CONFIGURE_DEPENDS "src/*.rst")



foreach(filename ${SPHINX_SOURCE_FILES})
    get_filename_component(fname ${filename} NAME)
    message(STATUS "Copying ${filename} to ${SPHINX_BUILD}/src/${fname}")
    configure_file(${filename} "${SPHINX_BUILD}/src/${fname}")  
endforeach(filename ${SPHINX_SOURCE_FILES})

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/conf.py.in"
    "${SPHINX_BUILD}/conf.py"
    @ONLY
)


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

add_custom_target(
    rst
    COMMAND ${SPHINX_EXECUTABLE} -a -b html
    -Dbreathe_projects.aare=${CMAKE_CURRENT_BINARY_DIR}/xml
    -c "${SPHINX_BUILD}"
    ${SPHINX_BUILD}/src 
    ${SPHINX_BUILD}/html
    COMMENT "Generating documentation with Sphinx"
)