

set(SourceFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/defs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Dtype.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Frame.cpp
)


add_library(aare_core STATIC ${SourceFiles})
target_include_directories(aare_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(aare_core PUBLIC fmt::fmt PRIVATE aare_compiler_flags )

if (AARE_PYTHON_BINDINGS)
set_property(TARGET aare_core PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

if(AARE_TESTS)
    set(TestSources
            ${CMAKE_CURRENT_SOURCE_DIR}/defs.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/Dtype.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/Frame.test.cpp
            # ${CMAKE_CURRENT_SOURCE_DIR}/test/ProducerConsumerQueue.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/NDArray.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/NDView.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/ClusterFinder.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/Pedestal.test.cpp
            # ${CMAKE_CURRENT_SOURCE_DIR}/test/CircularFifo.test.cpp
            # ${CMAKE_CURRENT_SOURCE_DIR}/test/wrappers.test.cpp
            # ${CMAKE_CURRENT_SOURCE_DIR}/test/Transforms.test.cpp

        )
    target_sources(tests PRIVATE ${TestSources} )
    target_link_libraries(tests PRIVATE aare_core)
endif()