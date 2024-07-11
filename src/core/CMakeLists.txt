

set(SourceFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/defs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Dtype.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Frame.cpp
)


add_library(core STATIC ${SourceFiles})
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)


target_link_libraries(core PUBLIC utils fmt::fmt PRIVATE aare_compiler_flags )

if (AARE_PYTHON_BINDINGS)
set_property(TARGET core PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

if(AARE_TESTS)
    set(TestSources
            ${CMAKE_CURRENT_SOURCE_DIR}/test/defs.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/Dtype.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/Frame.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/NDArray.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/NDView.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/wrappers.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/Transforms.test.cpp

        )
    target_sources(tests PRIVATE ${TestSources} )
    target_link_libraries(tests PRIVATE core utils)
endif()