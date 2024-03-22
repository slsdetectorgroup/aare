cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 17) #TODO! Global or per target?
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

#Set default build type if none was specified
include(cmake/helpers.cmake)
default_build_type("Release")

option(AARE_USE_WARNINGS "Eable warnings" ON)
option(AARE_PYTHON_BINDINGS "Build python bindings" ON)
option(AARE_TESTS "Build tests" ON)
option(AARE_EXAMPLES "Build examples" ON)



set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(fmt 6 REQUIRED)

add_library(aare_compiler_flags INTERFACE)
target_compile_features(aare_compiler_flags INTERFACE cxx_std_17)

#TODO! Explicitly setting flags is not cross platform compatible
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Release build")
    target_compile_options(aare_compiler_flags INTERFACE -O3)
else()
    target_compile_options(
        aare_compiler_flags 
        INTERFACE 
            -Og 
            -ggdb3 
            -D_GLIBCXX_DEBUG 
            -D_GLIBCXX_DEBUG_PEDANTIC
    )

    if (NOT AARE_PYTHON_BINDINGS)
    target_compile_options(
        aare_compiler_flags
        INTERFACE
            -fdiagnostics-parseable-fixits
            # -fdiagnostics-generate-patch 
            -fdiagnostics-show-template-tree 
            -fsanitize=address,undefined,pointer-compare 
            -fno-sanitize-recover 
            # -D_FORTIFY_SOURCE=2  # not needed for debug builds
            # -fstack-protector  # cause errors wih folly (ProducerConsumerQueue.hpp)
            -fno-omit-frame-pointer
    )
    
    target_link_libraries(
        aare_compiler_flags
        INTERFACE
            -fdiagnostics-parseable-fixits
            # -fdiagnostics-generate-patch 
            -fdiagnostics-show-template-tree 
            -fsanitize=address,undefined,pointer-compare 
            -fno-sanitize-recover 
            # -D_FORTIFY_SOURCE=2 
            -fno-omit-frame-pointer
    )
endif()

endif()

if(AARE_USE_WARNINGS)
    target_compile_options(
        aare_compiler_flags 
        INTERFACE 
            -Wall 
            -Wextra 
            -pedantic 
            -Wshadow 
            -Wnon-virtual-dtor
            -Woverloaded-virtual
            -Wdouble-promotion
            -Wformat=2
            -Wredundant-decls
            -Wvla
            -Wdouble-promotion
            -Werror=return-type #important can cause segfault in optimzed builds
        )
endif()

if(AARE_TESTS)
    add_subdirectory(tests)
endif()

add_subdirectory(core)
add_subdirectory(file_io)


#Overall target to link to when using the library
add_library(aare INTERFACE) 
target_link_libraries(aare INTERFACE core file_io)
target_include_directories(aare INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

add_subdirectory(examples)


if(AARE_PYTHON_BINDINGS)
    add_subdirectory(python)
endif()
