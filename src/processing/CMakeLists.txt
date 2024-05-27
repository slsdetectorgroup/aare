
add_library(processing STATIC src/Pedestal.cpp)
target_include_directories(processing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(processing PUBLIC core)


if(AARE_PYTHON_BINDINGS)
set_property(TARGET processing PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

if(AARE_TESTS)
    set(TestSources
            ${CMAKE_CURRENT_SOURCE_DIR}/test/Pedestal.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/ClusterFinder.test.cpp
        )
    target_sources(tests PRIVATE ${TestSources} )
    target_link_libraries(tests PRIVATE processing)
endif()