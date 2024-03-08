
find_package (Python 3.11 COMPONENTS Interpreter Development)
find_package(pybind11 2.11 REQUIRED)
pybind11_add_module(_aare src/bindings.cpp)
set_target_properties(_aare PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
target_link_libraries(_aare PRIVATE aare)