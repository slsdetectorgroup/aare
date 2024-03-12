FetchContent_Declare(
    Catch2
    GIT_SHALLOW    TRUE
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.3
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)

add_executable(tests test.cpp)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)

set_target_properties(tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        OUTPUT_NAME run_tests
)

include(CTest)
include(Catch)
catch_discover_tests(tests)

if(AARE_TESTS)
    set(TestSources
            ${CMAKE_CURRENT_SOURCE_DIR}/test.cpp
        )
    target_sources(tests PRIVATE ${TestSources} )

    #Work around to remove, this is not the way to do it =) 
    # target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/include/common)
    target_link_libraries(tests PRIVATE core aare_compiler_flags)
endif()