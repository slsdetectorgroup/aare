# SPDX-License-Identifier: MPL-2.0

# Each example is a standalone program that is also registered as a test, so
# ctest checks that it compiles, runs and returns success.
set(ExampleNames fit_gaussian)

foreach(name ${ExampleNames})
  add_executable(example_${name} ${name}.cpp)
  target_link_libraries(example_${name} PRIVATE aare_core aare_compiler_flags)
  add_test(NAME example_${name} COMMAND example_${name})
endforeach()
