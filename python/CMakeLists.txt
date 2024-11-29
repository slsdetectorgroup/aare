
find_package (Python 3.10 COMPONENTS Interpreter Development REQUIRED)

# Download or find pybind11 depending on configuration
if(AARE_FETCH_PYBIND11)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11
        GIT_TAG        v2.13.0
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



# Copy the examples/scripts to the build directory
configure_file(examples/play.py ${CMAKE_BINARY_DIR}/play.py)


if(AARE_INSTALL_PYTHONEXT)
    install(TARGETS _aare
        EXPORT "${TARGETS_EXPORT_NAME}"
        LIBRARY DESTINATION aare
    )

    install(FILES ${PYTHON_FILES} DESTINATION aare)
endif()