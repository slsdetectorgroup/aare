cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 17)
project(aare 
    VERSION 0.1
    DESCRIPTION "Data processing library for PSI detectors"
    HOMEPAGE_URL "https://github.com/slsdetectorgroup/aare"
    LANGUAGES C CXX 
)
# cmake_policy(SET CMP0135 NEW)

# include(GNUInstallDirs)

# include(cmake/helpers.cmake)
# default_build_type("Debug")]

set(CMAKE_BUILD_TYPE "Debug")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(OPTIMIZATION_FLAGS "-Og -ggdb3")
else()
    set(OPTIMIZATION_FLAGS "-O3")
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(USE_SANITIZER "Sanitizers for debugging" ON)
option(DISABLE_WARNINGS "Disbale compilation warnings" OFF)
# option(USE_TESTS "Unit tests" OFF)
# option(USE_PYTHON "Python bindings" OFF)
# option(TUNE_LOCAL "Tune to exact CPU architecture" OFF)
# option(BUILD_EXAMPLES "Build examples" OFF)
# option(DISABLE_LTO "Disable Link Time Optimizations" OFF)


# if(NOT APPLE)
#     set(CMAKE_INSTALL_RPATH $ORIGIN)
# endif()

set(OPTIONAL_FLAGS "")
if(DISABLE_WARNINGS)
    set(OPTIONAL_FLAGS "${OPTIONAL_FLAGS} -Wall -Wextra -pedantic -Wno-unused-parameter -Wshadow -Wformat=2 -Wold-style-cast -Wnon-virtual-dtor  -Wfloat-equal -Wconversion -Wlogical-op -Wshift-overflow=2 -Woverloaded-virtual -Winline")
endif()

if(USE_SANITIZER)
    set(OPTIONAL_FLAGS "${OPTIONAL_FLAGS} -fdiagnostics-parseable-fixits -fdiagnostics-generate-patch -fdiagnostics-show-template-tree -fsanitize=address,undefined,pointer-compare -fno-sanitize-recover -D_FORTIFY_SOURCE=2 -fstack-protector -fno-omit-frame-pointer ")
endif()

# if(TUNE_LOCAL)
#     if(UNIX AND NOT APPLE)
#         message(STATUS "unix")
#         set(ARCH_FLAGS )
#         target_compile_options(project_options INTERFACE -mtune=native -march=native )
#     elseif(APPLE)
#         message(STATUS "compiling for apple")
#         target_compile_options(project_options INTERFACE -mtune=apple-m1 -mcpu=apple-m1 )
#     endif()
#     # 
# endif()



#Enable LTO if available
include(CheckIPOSupported)
check_ipo_supported(RESULT LTO_AVAILABLE)
if((CMAKE_BUILD_TYPE STREQUAL "Release") AND  LTO_AVAILABLE)
    message(STATUS "Building with link time optimization")
else()
    message(STATUS "Building without link time optimization")
endif()


set(CMAKE_CXX_FLAGS  "${OPTIONAL_FLAGS} ${OPTIMIZATION_FLAGS}")

add_executable(aare)

add_subdirectory(src)
