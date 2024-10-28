if(AARE_FETCH_PYBIND11)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11
        GIT_TAG        v2.11.0
    )
    FetchContent_MakeAvailable(pybind11)
else()
    find_package(pybind11 2.11 REQUIRED)
endif()


pybind11_add_module(_aare src/module.cpp)
set_target_properties(_aare PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
target_link_libraries(_aare PRIVATE aare_core aare_compiler_flags)
