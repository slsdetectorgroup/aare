
if(AARE_FETCH_JSON)
    FetchContent_Declare(json
        GIT_REPOSITORY https://github.com/nlohmann/json
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(json)
else()
    find_package(nlohmann_json 3 REQUIRED)
endif()

set(SourceFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/File.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RawFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SubFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NumpyFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NumpyHelpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ClusterFile.cpp
)

add_library(file_io STATIC ${SourceFiles})
target_include_directories(file_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(file_io PRIVATE fmt::fmt core utils nlohmann_json::nlohmann_json simdjson aare_compiler_flags)

if(AARE_PYTHON_BINDINGS)
set_property(TARGET file_io PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()


if(AARE_TESTS)
    set(TestSources
            ${CMAKE_CURRENT_SOURCE_DIR}/test/NumpyFile.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/NumpyHelpers.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/RawFile.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/ClusterFile.test.cpp
        )
    target_sources(tests PRIVATE ${TestSources} )
    target_link_libraries(tests PRIVATE core file_io)
endif()