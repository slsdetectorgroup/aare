# Findh5py.cmake
#
# This module finds if h5py is installed and sets the H5PY_FOUND variable.
# It also sets the H5PY_INCLUDE_DIRS and H5PY_LIBRARIES variables.

find_package(PythonInterp REQUIRED)
find_package(PythonLibs REQUIRED)

execute_process(
    COMMAND ${PYTHON_EXECUTABLE} -c "import h5py"
    RESULT_VARIABLE H5PY_IMPORT_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
)

if(H5PY_IMPORT_RESULT EQUAL 0)
    set(H5PY_FOUND TRUE)
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE} -c "import h5py; print(h5py.get_include())"
        OUTPUT_VARIABLE H5PY_INCLUDE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(H5PY_INCLUDE_DIRS ${H5PY_INCLUDE_DIR})
    set(H5PY_LIBRARIES ${PYTHON_LIBRARIES})
else()
    set(H5PY_FOUND FALSE)
endif()

mark_as_advanced(H5PY_INCLUDE_DIRS H5PY_LIBRARIES)