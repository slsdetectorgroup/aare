add_library(utils STATIC src/logger.cpp)
target_include_directories(utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(utils PUBLIC core)

if (AARE_PYTHON_BINDINGS)
set_property(TARGET utils PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()


if(AARE_TESTS)
    set(TestSources
            ${CMAKE_CURRENT_SOURCE_DIR}/test/merge_frames.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/floats.test.cpp
        )
    target_sources(tests PRIVATE ${TestSources} )
    target_link_libraries(tests PRIVATE utils)

endif()
