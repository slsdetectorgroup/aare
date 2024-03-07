cmake_minimum_required(VERSION 3.12)
project(aare 
    VERSION 0.1
    DESCRIPTION "Data processing library for PSI detectors"
    HOMEPAGE_URL "https://github.com/slsdetectorgroup/aare"
    LANGUAGES C CXX 
)

cmake_policy(SET CMP0135 NEW)
cmake_policy(SET CMP0079 NEW)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(aare_compiler_flags INTERFACE)
target_compile_features(aare_compiler_flags INTERFACE cxx_std_17)

add_library(warning_compiler_flags INTERFACE)
add_library(debug_compiler_flags INTERFACE)
add_library(sanitizer_compiler_flags INTERFACE)




include(GNUInstallDirs)

include(FetchContent)


option(USE_SANITIZER "Sanitizers for debugging" ON)
option(DISABLE_WARNINGS "Disbale compilation warnings" OFF)
option(USE_PYTHON "Build python bindings" ON)
option(AARE_BUILD_TESTS "Build tests" OFF)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

if(AARE_BUILD_TESTS)
    add_subdirectory(tests)
endif()



find_package(fmt 6 REQUIRED)


set(CMAKE_BUILD_TYPE "Debug")


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(debug_compiler_flags INTERFACE -Og -ggdb3 -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC)
else()
    target_compile_options(debug_compiler_flags INTERFACE -O3)
endif()





if(DISABLE_WARNINGS)
target_compile_options(warning_compiler_flags INTERFACE -Wall -Wextra -pedantic -Wno-unused-parameter -Wshadow -Wformat=2 -Wold-style-cast -Wnon-virtual-dtor  -Wfloat-equal -Wconversion -Wlogical-op -Wshift-overflow=2 -Woverloaded-virtual -Winline)
endif()

if(USE_SANITIZER)
target_compile_options(sanitizer_compiler_flags INTERFACE  -fPIC)
endif()

# each library can choose to use sanitizer flags or not

target_link_libraries(aare_compiler_flags
 INTERFACE
  warning_compiler_flags
  debug_compiler_flags
  sanitizer_compiler_flags
  )

include_directories(include)
add_subdirectory(src)


add_library(aare INTERFACE) 
target_link_libraries(aare INTERFACE common core file_io)


add_subdirectory(examples)
target_link_libraries(example PUBLIC aare)


if(USE_PYTHON)
    add_subdirectory(python)
endif()

