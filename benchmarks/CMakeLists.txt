find_package(benchmark REQUIRED)

add_executable(ndarray_benchmark ndarray_benchmark.cpp)

target_link_libraries(ndarray_benchmark benchmark::benchmark aare_core aare_compiler_flags)
# target_link_libraries(tests PRIVATE aare_core aare_compiler_flags)

set_target_properties(ndarray_benchmark PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        # OUTPUT_NAME run_tests
)