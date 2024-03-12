

set(SourceFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/defs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Frame.cpp
)

add_library(core STATIC ${SourceFiles})
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)


target_link_libraries(core PUBLIC fmt::fmt PRIVATE aare_compiler_flags)
set_property(TARGET core PROPERTY POSITION_INDEPENDENT_CODE ON)


if(AARE_TESTS)
    set(TestSources
            ${CMAKE_CURRENT_SOURCE_DIR}/src/defs.test.cpp
        )
    target_sources(tests PRIVATE ${TestSources} )
    target_link_libraries(tests PRIVATE core)
endif()