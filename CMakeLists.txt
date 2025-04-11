cmake_minimum_required(VERSION 3.15)

project(aare 
    VERSION 1.0.0
    DESCRIPTION "Data processing library for PSI detectors"
    HOMEPAGE_URL "https://github.com/slsdetectorgroup/aare"
    LANGUAGES C CXX 
)

set(CMAKE_CXX_STANDARD 17) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

execute_process(
        COMMAND git log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
message(STATUS "Building from git hash: ${GIT_HASH}")

if (${CMAKE_VERSION} VERSION_GREATER "3.24")
    cmake_policy(SET CMP0135 NEW) #Fetch content download timestamp
endif()
cmake_policy(SET CMP0079 NEW)

include(GNUInstallDirs)
include(FetchContent)

#Set default build type if none was specified
include(cmake/helpers.cmake)


default_build_type("Release")
set_std_fs_lib()
message(STATUS "Extra linking to fs lib:${STD_FS_LIB}")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})


# General options
option(AARE_PYTHON_BINDINGS "Build python bindings" ON)
option(AARE_TESTS "Build tests" OFF)
option(AARE_BENCHMARKS "Build benchmarks" OFF)
option(AARE_EXAMPLES "Build examples" OFF)
option(AARE_IN_GITHUB_ACTIONS "Running in Github Actions" OFF)
option(AARE_DOCS "Build documentation" OFF)
option(AARE_VERBOSE "Verbose output" OFF)
option(AARE_CUSTOM_ASSERT "Use custom assert" OFF)
option(AARE_INSTALL_PYTHONEXT "Install the python extension in the install tree under CMAKE_INSTALL_PREFIX/aare/" OFF)
option(AARE_ASAN "Enable AddressSanitizer" OFF)

# Configure which of the dependencies to use FetchContent for
option(AARE_FETCH_FMT "Use FetchContent to download fmt" ON)
option(AARE_FETCH_PYBIND11 "Use FetchContent to download pybind11" ON)
option(AARE_FETCH_CATCH "Use FetchContent to download catch2" ON)
option(AARE_FETCH_JSON "Use FetchContent to download nlohmann::json" ON)
option(AARE_FETCH_ZMQ "Use FetchContent to download libzmq" ON)
option(AARE_FETCH_LMFIT "Use FetchContent to download lmfit" ON)


#Convenience option to use system libraries only (no FetchContent)
option(AARE_SYSTEM_LIBRARIES "Use system libraries" OFF)
if(AARE_SYSTEM_LIBRARIES)
    message(STATUS "Build using system libraries")
    set(AARE_FETCH_FMT OFF CACHE BOOL "Disabled FetchContent for FMT" FORCE)
    set(AARE_FETCH_PYBIND11 OFF CACHE BOOL "Disabled FetchContent for pybind11" FORCE)
    set(AARE_FETCH_CATCH OFF CACHE BOOL "Disabled FetchContent for catch2" FORCE)
    set(AARE_FETCH_JSON OFF CACHE BOOL "Disabled FetchContent for nlohmann::json" FORCE)
    set(AARE_FETCH_ZMQ OFF CACHE BOOL "Disabled FetchContent for libzmq" FORCE)
    # Still fetch lmfit when setting AARE_SYSTEM_LIBRARIES since this is not available
    # on conda-forge
endif()

if(AARE_VERBOSE)
    add_compile_definitions(AARE_VERBOSE)
endif()

if(AARE_CUSTOM_ASSERT)
    add_compile_definitions(AARE_CUSTOM_ASSERT)
endif()

if(AARE_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(AARE_FETCH_LMFIT)
    #TODO! Should we fetch lmfit from the web or inlcude a tar.gz in the repo?
    set(LMFIT_PATCH_COMMAND git apply ${CMAKE_CURRENT_SOURCE_DIR}/patches/lmfit.patch)

    # For cmake < 3.28 we can't supply EXCLUDE_FROM_ALL to FetchContent_Declare
    # so we need this workaround
    if (${CMAKE_VERSION} VERSION_LESS "3.28")
        FetchContent_Declare(
            lmfit
            GIT_REPOSITORY https://jugit.fz-juelich.de/mlz/lmfit.git
            GIT_TAG        main
            PATCH_COMMAND ${LMFIT_PATCH_COMMAND}
            UPDATE_DISCONNECTED 1
        )
    else()
        FetchContent_Declare(
            lmfit
            GIT_REPOSITORY https://jugit.fz-juelich.de/mlz/lmfit.git
            GIT_TAG        main
            PATCH_COMMAND ${LMFIT_PATCH_COMMAND}
            UPDATE_DISCONNECTED 1
            EXCLUDE_FROM_ALL 1
        )
    endif()
    

    #Disable what we don't need from lmfit
    set(BUILD_TESTING OFF CACHE BOOL "")
    set(LMFIT_CPPTEST OFF CACHE BOOL "")
    set(LIB_MAN OFF CACHE BOOL "")
    set(LMFIT_CPPTEST OFF CACHE BOOL "")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "")

    if (${CMAKE_VERSION} VERSION_LESS "3.28")
        if(NOT lmfit_POPULATED)
            FetchContent_Populate(lmfit)
            add_subdirectory(${lmfit_SOURCE_DIR} ${lmfit_BINARY_DIR} EXCLUDE_FROM_ALL)
        endif()
    else()
        FetchContent_MakeAvailable(lmfit)
    endif()
    
    set_property(TARGET lmfit PROPERTY POSITION_INDEPENDENT_CODE ON)
else()
    find_package(lmfit REQUIRED)
endif()


if(AARE_FETCH_ZMQ)
    # Fetchcontent_Declare is deprecated need to find a way to update this
    # for now setting the policy to old is enough
    if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.30")
        cmake_policy(SET CMP0169 OLD)
    endif()
    set(ZMQ_PATCH_COMMAND git apply ${CMAKE_CURRENT_SOURCE_DIR}/patches/libzmq_cmake_version.patch)
    FetchContent_Declare(
        libzmq
        GIT_REPOSITORY https://github.com/zeromq/libzmq.git
        GIT_TAG        v4.3.4
        PATCH_COMMAND ${ZMQ_PATCH_COMMAND}
        UPDATE_DISCONNECTED 1
    )
    # Disable unwanted options from libzmq
    set(BUILD_TESTS OFF CACHE BOOL "Switch off libzmq test build")
    set(BUILD_SHARED OFF CACHE BOOL "Switch off libzmq shared libs")
    set(WITH_PERF_TOOL OFF CACHE BOOL "")
    set(ENABLE_CPACK OFF CACHE BOOL "")
    set(ENABLE_CLANG OFF CACHE BOOL "")
    set(ENABLE_CURVE OFF CACHE BOOL "")
    set(ENABLE_DRAFTS OFF CACHE BOOL "")

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
    set(FMT_INSTALL ON CACHE BOOL "")
    # set(FMT_CMAKE_DIR "")
    FetchContent_MakeAvailable(fmt)
    set_property(TARGET fmt PROPERTY POSITION_INDEPENDENT_CODE ON)
    install(TARGETS fmt 
        EXPORT ${project}-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}   
    )  
else()
      find_package(fmt 6 REQUIRED)
endif()


if (AARE_FETCH_JSON)
    FetchContent_Declare(
        json 
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    set(JSON_Install ON CACHE BOOL "")
    FetchContent_MakeAvailable(json)
    set(NLOHMANN_JSON_TARGET_NAME nlohmann_json)
    
    install(
        TARGETS nlohmann_json
        EXPORT "${TARGETS_EXPORT_NAME}"
    )
    message(STATUS "target: ${NLOHMANN_JSON_TARGET_NAME}")
else()
    find_package(nlohmann_json 3.11.3 REQUIRED)
endif()

include(GNUInstallDirs)

# If conda build, always set lib dir to 'lib'
if($ENV{CONDA_BUILD})
  set(CMAKE_INSTALL_LIBDIR "lib")
endif()

# Set lower / upper case project names
string(TOUPPER "${PROJECT_NAME}" PROJECT_NAME_UPPER)
string(TOLOWER "${PROJECT_NAME}" PROJECT_NAME_LOWER)


# Set targets export name (used by slsDetectorPackage and dependencies)
set(TARGETS_EXPORT_NAME "${PROJECT_NAME_LOWER}-targets")
set(namespace "aare::")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Check if project is being used directly or via add_subdirectory
set(AARE_MASTER_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(AARE_MASTER_PROJECT ON)
endif()

add_library(aare_compiler_flags INTERFACE)
target_compile_features(aare_compiler_flags INTERFACE cxx_std_17)

if(AARE_PYTHON_BINDINGS)
    add_subdirectory(python)
endif()

#################
# MSVC specific #
#################
if(MSVC)
add_compile_definitions(AARE_MSVC)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Release build")
    target_compile_options(aare_compiler_flags INTERFACE /O2)
else()
    message(STATUS "Debug build")
    target_compile_options(
        aare_compiler_flags 
        INTERFACE 
            /Od 
            /Zi 
            /MDd
            /D_ITERATOR_DEBUG_LEVEL=2
    )
    target_link_options(
        aare_compiler_flags
        INTERFACE
            /DEBUG:FULL
            )
endif()
target_compile_options(
    aare_compiler_flags 
    INTERFACE
    /w # disable warnings
)


else()
######################
# GCC/Clang specific #
######################

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Release build")
    target_compile_options(aare_compiler_flags INTERFACE -O3)
else()
    message(STATUS "Debug build")
endif()

# Common flags for GCC and Clang
target_compile_options(
    aare_compiler_flags 
    INTERFACE 
        -Wall 
        -Wextra 
        -pedantic 
        -Wshadow 
        -Wold-style-cast
        -Wnon-virtual-dtor
        -Woverloaded-virtual
        -Wdouble-promotion
        -Wformat=2
        -Wredundant-decls
        -Wvla
        -Wdouble-promotion
        -Werror=return-type #important can cause segfault in optimzed builds
    )

endif() #GCC/Clang specific


if(AARE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    target_compile_options(
        aare_compiler_flags 
        INTERFACE 
            -fsanitize=address,undefined,pointer-compare 
            -fno-omit-frame-pointer
    )
    target_link_libraries(
        aare_compiler_flags 
        INTERFACE 
            -fsanitize=address,undefined,pointer-compare 
            -fno-omit-frame-pointer
    )
endif()





if(AARE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

###------------------------------------------------------------------------------MAIN LIBRARY
###------------------------------------------------------------------------------------------

set(PUBLICHEADERS
    include/aare/ArrayExpr.hpp
    include/aare/ClusterFinder.hpp
    include/aare/ClusterFile.hpp
    include/aare/CtbRawFile.hpp
    include/aare/ClusterVector.hpp
    include/aare/decode.hpp
    include/aare/defs.hpp
    include/aare/Dtype.hpp
    include/aare/File.hpp
    include/aare/Fit.hpp
    include/aare/FileInterface.hpp
    include/aare/FilePtr.hpp
    include/aare/Frame.hpp
    include/aare/geo_helpers.hpp
    include/aare/JungfrauDataFile.hpp
    include/aare/NDArray.hpp
    include/aare/NDView.hpp
    include/aare/NumpyFile.hpp
    include/aare/NumpyHelpers.hpp
    include/aare/Pedestal.hpp
    include/aare/PixelMap.hpp
    include/aare/RawFile.hpp
    include/aare/RawMasterFile.hpp
    include/aare/RawSubFile.hpp
    include/aare/VarClusterFinder.hpp
    include/aare/utils/task.hpp

)


set(SourceFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CtbRawFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ClusterFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/defs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Dtype.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/decode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Frame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/File.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FilePtr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Fit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/geo_helpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/JungfrauDataFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NumpyFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NumpyHelpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Interpolator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PixelMap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RawFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RawSubFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RawMasterFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/task.cpp
)


add_library(aare_core STATIC ${SourceFiles})
target_include_directories(aare_core PUBLIC 
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)



target_link_libraries(
    aare_core 
    PUBLIC 
    fmt::fmt 
    nlohmann_json::nlohmann_json 
    ${STD_FS_LIB} # from helpers.cmake
    PRIVATE 
    aare_compiler_flags 
    "$<BUILD_INTERFACE:lmfit>"

)

set_target_properties(aare_core PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    PUBLIC_HEADER "${PUBLICHEADERS}"
)

if (AARE_PYTHON_BINDINGS)
    set_property(TARGET aare_core PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

if(AARE_TESTS)
    set(TestSources
            ${CMAKE_CURRENT_SOURCE_DIR}/src/algorithm.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/defs.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/Dtype.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/Frame.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/geo_helpers.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/RawMasterFile.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/NDArray.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/NDView.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ClusterFinder.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ClusterVector.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ClusterFile.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/Pedestal.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/JungfrauDataFile.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/NumpyFile.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/NumpyHelpers.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/RawFile.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/task.test.cpp

        )
    target_sources(tests PRIVATE ${TestSources} )
endif()



###------------------------------------------------------------------------------------------
###------------------------------------------------------------------------------------------


if(AARE_MASTER_PROJECT)
    install(TARGETS aare_core aare_compiler_flags 
        EXPORT "${TARGETS_EXPORT_NAME}"
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/aare
    )
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INSTALL_RPATH $ORIGIN)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)


# #Overall target to link to when using the library
# add_library(aare INTERFACE) 
# target_link_libraries(aare INTERFACE aare_core aare_compiler_flags)
# target_include_directories(aare INTERFACE 
#     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
#     $<INSTALL_INTERFACE:include>
# )

# add_subdirectory(examples)

if(AARE_DOCS)
    add_subdirectory(docs)
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
    COMMAND find \( -path "./src/*" -a -not -path "./src/python/*" -a \( -name "*.cpp"   -not -name "*.test.cpp" \)  \)    -not -name "CircularFifo.hpp" -not -name "ProducerConsumerQueue.hpp" -not -name "VariableSizeClusterFinder.hpp" | xargs -I {} -n 1 -P 10 bash -c "${CLANG_TIDY_COMMAND} --config-file=.clang-tidy -p build {}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "linting with clang-tidy"
    VERBATIM
)

if(AARE_MASTER_PROJECT)
    set(CMAKE_INSTALL_DIR "share/cmake/${PROJECT_NAME}")
    set(PROJECT_LIBRARIES aare-core aare-compiler-flags )
    include(cmake/package_config.cmake)
endif()
