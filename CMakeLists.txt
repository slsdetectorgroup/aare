cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 17)
project(aare 
    VERSION 0.1
    DESCRIPTION "Data processing library for PSI detectors"
    HOMEPAGE_URL "https://github.com/slsdetectorgroup/aare"
    LANGUAGES C CXX 
)

cmake_policy(SET CMP0135 NEW)
cmake_policy(SET CMP0079 NEW)


include(GNUInstallDirs)

include(FetchContent)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

find_package(fmt 6 REQUIRED)


set(CMAKE_BUILD_TYPE "Debug")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(OPTIMIZATION_FLAGS "-Og -ggdb3 -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC")
else()
    set(OPTIMIZATION_FLAGS "-O3")
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(USE_SANITIZER "Sanitizers for debugging" ON)
option(DISABLE_WARNINGS "Disbale compilation warnings" OFF)
option(USE_PYTHON "Build python bindings" ON)


set(OPTIONAL_FLAGS "")
if(DISABLE_WARNINGS)
    set(OPTIONAL_FLAGS "${OPTIONAL_FLAGS} -Wall -Wextra -pedantic -Wno-unused-parameter -Wshadow -Wformat=2 -Wold-style-cast -Wnon-virtual-dtor  -Wfloat-equal -Wconversion -Wlogical-op -Wshift-overflow=2 -Woverloaded-virtual -Winline")
endif()

if(USE_SANITIZER)
    set(OPTIONAL_FLAGS "${OPTIONAL_FLAGS} -fdiagnostics-parseable-fixits -fdiagnostics-generate-patch -fdiagnostics-show-template-tree -fsanitize=address,undefined,pointer-compare -fno-sanitize-recover -D_FORTIFY_SOURCE=2 -fstack-protector -fno-omit-frame-pointer ")
endif()



set(SUPPRESSED_WARNINGS "-Wno-return-type")

set(CMAKE_CXX_FLAGS  "${OPTIMIZATION_FLAGS} ${SUPPRESSED_WARNINGS}")

if(USE_PYTHON)
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fPIC")
endif(USE_PYTHON)




include_directories(include)
add_subdirectory(src)


add_library(aare INTERFACE) 
target_link_libraries(aare INTERFACE common core file_io)


add_subdirectory(examples)
target_link_libraries(example PUBLIC aare)

if(USE_PYTHON)
    find_package (Python 3.11 COMPONENTS Interpreter Development)
    find_package(pybind11 2.11 REQUIRED)
    add_subdirectory(python)
    target_link_libraries(_aare PRIVATE aare nlohmann_json::nlohmann_json fmt::fmt)
endif()