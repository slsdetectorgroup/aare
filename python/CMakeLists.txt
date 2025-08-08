
find_package (Python 3.10 COMPONENTS Interpreter Development.Module REQUIRED)
set(PYBIND11_FINDPYTHON ON) # Needed for RH8

# Download or find pybind11 depending on configuration
if(AARE_FETCH_PYBIND11)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11
        GIT_TAG        v2.13.6
    )
    FetchContent_MakeAvailable(pybind11)
else()
    find_package(pybind11 2.13 REQUIRED)
endif()

# Add the compiled python extension
pybind11_add_module(
    _aare           # name of the module
    src/module.cpp  # source file
    )

set_target_properties(_aare PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
target_link_libraries(_aare PRIVATE aare_core aare_compiler_flags)

# List of python files to be copied to the build directory
set( PYTHON_FILES
    aare/__init__.py
    aare/CtbRawFile.py
    aare/ClusterFinder.py
    aare/ClusterVector.py

    aare/calibration.py
    aare/func.py
    aare/RawFile.py
    aare/transform.py
    aare/ScanParameters.py
    aare/utils.py
)


# Copy the python files to the build directory
foreach(FILE ${PYTHON_FILES})
    configure_file(${FILE} ${CMAKE_BINARY_DIR}/${FILE}  )
endforeach(FILE ${PYTHON_FILES})

set_target_properties(_aare PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/aare
)

set(PYTHON_EXAMPLES
    examples/play.py
    examples/fits.py
)


# Copy the python examples to the build directory
foreach(FILE ${PYTHON_EXAMPLES})
    configure_file(${FILE} ${CMAKE_BINARY_DIR}/${FILE}  )
    message(STATUS "Copying ${FILE} to ${CMAKE_BINARY_DIR}/${FILE}")
endforeach(FILE ${PYTHON_EXAMPLES})


if(AARE_INSTALL_PYTHONEXT)
    install(
        TARGETS _aare
        EXPORT "${TARGETS_EXPORT_NAME}"
        LIBRARY DESTINATION aare
        COMPONENT python
    )

    install(
        FILES ${PYTHON_FILES} 
        DESTINATION aare
        COMPONENT python
        )
endif()