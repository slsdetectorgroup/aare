FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

set(SourceFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/File.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/helpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RawFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RawFileFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SubFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NumpyFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NumpyFileFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NumpyHelpers.cpp
)

add_library(file_io STATIC ${SourceFiles})
target_include_directories(file_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(file_io PRIVATE fmt::fmt core utils nlohmann_json::nlohmann_json aare_compiler_flags)

if(AARE_PYTHON_BINDINGS)
set_property(TARGET file_io PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()


if(AARE_TESTS)
    set(TestSources
            ${CMAKE_CURRENT_SOURCE_DIR}/src/NumpyHelpers.test.cpp
        )
    target_sources(tests PRIVATE ${TestSources} )
    target_link_libraries(tests PRIVATE core file_io)
endif()