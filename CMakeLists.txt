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


option(AARE_USE_SANITIZER "Sanitizers for debugging" ON)
option(AARE_PYTHON_BINDINGS "Build python bindings" ON)
option(AARE_TESTS "Build tests" OFF)
option(AARE_EXAMPLES "Build examples" OFF)

#TODO! Should this be on the top level or move it down to the component
#that needs it? 
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)



set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(fmt 6 REQUIRED)


# set(CMAKE_BUILD_TYPE "Debug")

# if (CMAKE_BUILD_TYPE STREQUAL "Debug")
#     set(OPTIMIZATION_FLAGS "-Og -ggdb3 -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC")
# else()
#     set(OPTIMIZATION_FLAGS "-O3")
# endif()





# set(OPTIONAL_FLAGS "")
# if(DISABLE_WARNINGS)
#     set(OPTIONAL_FLAGS "${OPTIONAL_FLAGS} -Wall -Wextra -pedantic -Wno-unused-parameter -Wshadow -Wformat=2 -Wold-style-cast -Wnon-virtual-dtor  -Wfloat-equal -Wconversion -Wlogical-op -Wshift-overflow=2 -Woverloaded-virtual -Winline")
# endif()

# if(USE_SANITIZER)
#     set(OPTIONAL_FLAGS "${OPTIONAL_FLAGS} -fdiagnostics-parseable-fixits -fdiagnostics-generate-patch -fdiagnostics-show-template-tree -fsanitize=address,undefined,pointer-compare -fno-sanitize-recover -D_FORTIFY_SOURCE=2 -fstack-protector -fno-omit-frame-pointer ")
# endif()



# set(SUPPRESSED_WARNINGS "-Wno-return-type")

# set(CMAKE_CXX_FLAGS  "${OPTIMIZATION_FLAGS} ${SUPPRESSED_WARNINGS}")


if(AARE_BUILD_TESTS)
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
target_link_libraries(example PUBLIC aare)


if(AARE_PYTHON_BINDINGS)
    add_subdirectory(python)
endif()