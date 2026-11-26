# SPDX-License-Identifier: MPL-2.0

include(FetchContent)

if(AARE_FETCH_BENCHMARK)
  aare_declare_dependency(
    benchmark
    # https://github.com/google/benchmark/archive/refs/tags/v1.8.3.tar.gz
    ARCHIVE benchmark-1.8.3.tar.gz
    SHA256 6bc180a57d23d4d9515519f92b0c83d61b05b5bab188961f36ac7b06b0d9e9ce
    GITHUB GIT_REPOSITORY https://github.com/google/benchmark.git
           GIT_TAG v1.8.3
           GIT_SHALLOW TRUE)

  # Ensure Google Benchmark is built correctly
  set(BENCHMARK_ENABLE_TESTING
      OFF
      CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(benchmark)
else()
  find_package(benchmark REQUIRED)
endif()

add_executable(benchmarks)

target_sources(
  benchmarks
  PRIVATE ndarray_benchmark.cpp
          ndview_benchmark.cpp
          calculateeta_benchmark.cpp
          reduce_benchmark.cpp
          clusterfile_benchmark.cpp
          DataSetAggregation_benchmark.cpp
          rawfile_benchmark.cpp
          varclusterfinder_benchmark.cpp)

# Link Google Benchmark and other necessary libraries
target_link_libraries(benchmarks PRIVATE benchmark::benchmark aare_core
                                         aare_compiler_flags)

# Set output properties
set_target_properties(
  benchmarks PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
                        OUTPUT_NAME run_benchmarks)

add_executable(fit_benchmark fit_benchmark.cpp)
target_link_libraries(fit_benchmark PRIVATE benchmark::benchmark aare_core
                                            aare_compiler_flags)
target_include_directories(
  fit_benchmark SYSTEM
  PRIVATE $<TARGET_PROPERTY:Minuit2::Minuit2,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(fit_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                               ${CMAKE_BINARY_DIR})
