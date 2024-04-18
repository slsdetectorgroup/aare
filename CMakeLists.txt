
cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 17) #TODO! Global or per target?
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
option(AARE_IN_GITHUB_ACTIONS "Running in Github Actions" OFF)

option(AARE_FETCH_FMT "Use FetchContent to download fmt" ON)
option(AARE_FETCH_PYBIND11 "Use FetchContent to download pybind11" ON)
option(AARE_FETCH_CATCH "Use FetchContent to download catch2" ON)
option(AARE_FETCH_JSON "Use FetchContent to download nlohmann::json" ON)
option(AARE_FETCH_ZMQ "Use FetchContent to download libzmq" ON)
option(AARE_FETCH_BOOST "Use FetchContent to download boost" ON)


#Convenience option to use system libraries
option(AARE_SYSTEM_LIBRARIES "Use system libraries" OFF)
if(AARE_SYSTEM_LIBRARIES)
    message(STATUS "Build using system libraries")
    set(AARE_FETCH_FMT OFF CACHE BOOL "Disabled FetchContent for FMT" FORCE)
    set(AARE_FETCH_PYBIND11 OFF CACHE BOOL "Disabled FetchContent for pybind11" FORCE)
    set(AARE_FETCH_CATCH OFF CACHE BOOL "Disabled FetchContent for catch2" FORCE)
    set(AARE_FETCH_JSON OFF CACHE BOOL "Disabled FetchContent for nlohmann::json" FORCE)
    set(AARE_FETCH_ZMQ OFF CACHE BOOL "Disabled FetchContent for libzmq" FORCE)
    set(AARE_FETCH_BOOST OFF CACHE BOOL "Disabled FetchContent for boost" FORCE)

endif()


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(AARE_FETCH_ZMQ)
    FetchContent_Declare(
        libzmq
        GIT_REPOSITORY https://github.com/zeromq/libzmq.git
        GIT_TAG        v4.3.4
    )
    # TODO! Verify that this is what we want to do in aare
    # Using GetProperties and Populate to be able to exclude zmq
    # from install (not possible with FetchContent_MakeAvailable(libzmq))
    FetchContent_GetProperties(libzmq)
    if(NOT libzmq_POPULATED)
        FetchContent_Populate(libzmq)
        add_subdirectory(${libzmq_SOURCE_DIR} ${libzmq_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
else()
    find_package(ZeroMQ 4 REQUIRED)
endif()


if (AARE_FETCH_FMT)
    set(FMT_TEST OFF CACHE INTERNAL "disabling fmt tests")
    FetchContent_Declare(
            fmt
            GIT_REPOSITORY  https://github.com/fmtlib/fmt.git
            GIT_TAG         10.2.1
            GIT_PROGRESS    TRUE
            USES_TERMINAL_DOWNLOAD TRUE
    )
    FetchContent_MakeAvailable(fmt)
else()
      find_package(fmt 6 REQUIRED)
endif()


if (AARE_FETCH_BOOST)
    set(BOOST_INCLUDE_LIBRARIES program_options)
    set(BOOST_ENABLE_CMAKE ON)
    FetchContent_Declare(
    Boost
    GIT_REPOSITORY https://github.com/boostorg/boost.git
    GIT_TAG boost-1.80.0
    )
    FetchContent_MakeAvailable(Boost)
    set(Boost_LIBRARIES Boost::program_options)
else()

    find_package(Boost 1.80 REQUIRED COMPONENTS program_options)
endif()



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
            # -D_GLIBCXX_DEBUG # causes errors with boost
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
    enable_testing()
    add_subdirectory(tests)
endif()

add_subdirectory(core)
add_subdirectory(file_io)
add_subdirectory(utils)
add_subdirectory(network_io)


#Overall target to link to when using the library
add_library(aare INTERFACE) 
target_link_libraries(aare INTERFACE core file_io utils network_io ${Boost_LIBRARIES})
target_include_directories(aare INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

add_subdirectory(examples)


if(AARE_PYTHON_BINDINGS)
    add_subdirectory(python)
endif()


# custom target to run check formatting with clang-format
add_custom_target(
    check-format
    COMMAND find \( -name "*.cpp" -o -name "*.hpp" \) -not -path "./build/*" | xargs -I {} -n 1 -P 10 bash -c  "clang-format -Werror -style=\"file:.clang-format\" {} | diff {} -"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking code formatting with clang-format"
    VERBATIM
    
)

add_custom_target(
    format-files
    COMMAND find \( -name "*.cpp" -o -name "*.hpp" \) -not -path "./build/*" | xargs -I {} -n 1 -P 10 bash -c "clang-format -i -style=\"file:.clang-format\" {}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Formatting with clang-format"
    VERBATIM
)

if (AARE_IN_GITHUB_ACTIONS)
    message(STATUS "Running in Github Actions")
    set(CLANG_TIDY_COMMAND "clang-tidy-17")
else()
    set(CLANG_TIDY_COMMAND "clang-tidy")
endif()

add_custom_target(
    clang-tidy
    COMMAND find \( -path "./core/*" -o -path "./file_io/*" -o -path "./network_io/*" -o -path "./utils/*" \) -name "*.cpp" -not -name "*.test.cpp" -not -name "CircularFifo.hpp" -not -name "ProducerConsumerQueue.hpp" -not -name "VariableSizeClusterFinder.hpp" | xargs -I {} -n 1 -P 10 bash -c "${CLANG_TIDY_COMMAND} --config-file=.clang-tidy -p build {}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "linting with clang-tidy"
    VERBATIM
)

