
find_package (Python 3.11 COMPONENTS Interpreter Development)



if(AARE_FETCH_PYBIND11)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11
        GIT_TAG        v2.13.1
    )
    FetchContent_MakeAvailable(pybind11)
else()
    find_package(pybind11 2.13.1 REQUIRED)
endif()





pybind11_add_module(_aare cpp/bindings.cpp)
set_target_properties(_aare PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
target_link_libraries(_aare PRIVATE aare aare_compiler_flags)



if(AARE_TESTS)
    set(TestSources
            ${CMAKE_CURRENT_SOURCE_DIR}/test/DtypeFormatDescr.test.cpp
        )
    target_sources(tests PRIVATE ${TestSources} )
    target_link_libraries(tests PRIVATE core pybind11::embed)

endif()
