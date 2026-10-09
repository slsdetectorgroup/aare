# SPDX-License-Identifier: MPL-2.0

# Download catch2 if configured to do so
if(AARE_FETCH_CATCH)
  FetchContent_Declare(
    Catch2
    GIT_SHALLOW TRUE
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.3)
  FetchContent_MakeAvailable(Catch2)
  # Catch2 is ~100 small files; a unity build compiles them in a few batches
  set_target_properties(Catch2 Catch2WithMain
                        PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 16)
else()
  # Otherwise look for installed catch2
  find_package(Catch2 3 REQUIRED)
endif()

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)

add_executable(tests test.cpp)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain aare_core
                                    aare_compiler_flags)
# target_compile_options(tests PRIVATE -fno-omit-frame-pointer
# -fsanitize=address)
set_target_properties(
  tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR} OUTPUT_NAME
                                                                run_tests)

# Headers nearly every test includes, parsed once instead of per test file. Keep
# this list to the test framework and ubiquitous aare headers: anything listed
# here is available in every test without an #include.
target_precompile_headers(
  tests
  PRIVATE
  <catch2/catch_test_macros.hpp>
  <catch2/matchers/catch_matchers_floating_point.hpp>
  <fmt/format.h>
  <filesystem>
  <fstream>
  <string>
  <vector>
  <array>
  <thread>
  <chrono>
  "${PROJECT_SOURCE_DIR}/include/aare/NDArray.hpp"
  "${PROJECT_SOURCE_DIR}/include/aare/NDView.hpp"
  "${PROJECT_SOURCE_DIR}/include/aare/defs.hpp"
  "${PROJECT_SOURCE_DIR}/include/aare/Frame.hpp"
  "${PROJECT_SOURCE_DIR}/include/aare/Dtype.hpp")

include(CTest)
include(Catch)
catch_discover_tests(tests)

set(TestSources ${CMAKE_CURRENT_SOURCE_DIR}/test.cpp)
target_sources(tests PRIVATE ${TestSources})

# Work around to remove, this is not the way to do it =)
# target_link_libraries(tests PRIVATE aare_core aare_compiler_flags)

# configure a header to pass test file paths
get_filename_component(TEST_FILE_PATH ${PROJECT_SOURCE_DIR}/data ABSOLUTE)
configure_file(test_config.hpp.in test_config.hpp)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                         ${CMAKE_CURRENT_BINARY_DIR})
