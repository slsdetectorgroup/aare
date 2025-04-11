
include(FetchContent)


FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.8.3 # Change to the latest version if needed
)

# Ensure Google Benchmark is built correctly
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(benchmark)

add_executable(benchmarks)

target_sources(benchmarks PRIVATE ndarray_benchmark.cpp calculateeta_benchmark.cpp)

# Link Google Benchmark and other necessary libraries
target_link_libraries(benchmarks PRIVATE benchmark::benchmark aare_core aare_compiler_flags)

# Set output properties
set_target_properties(benchmarks PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    OUTPUT_NAME run_benchmarks
)