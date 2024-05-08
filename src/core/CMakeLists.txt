

set(SourceFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/defs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DType.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Frame.cpp
)


add_library(core STATIC ${SourceFiles})
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)


target_link_libraries(core PUBLIC fmt::fmt PRIVATE aare_compiler_flags utils )

if (AARE_PYTHON_BINDINGS)
set_property(TARGET core PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

if(AARE_TESTS)
    set(TestSources
            ${CMAKE_CURRENT_SOURCE_DIR}/test/defs.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/DType.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/Frame.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/ProducerConsumerQueue.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/NDArray.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/NDView.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/CircularFifo.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/wrappers.test.cpp

        )
    target_sources(tests PRIVATE ${TestSources} )
    target_link_libraries(tests PRIVATE core utils)
endif()