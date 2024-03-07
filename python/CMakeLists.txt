pybind11_add_module(_aare src/bindings.cpp)
set_target_properties(_aare PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)