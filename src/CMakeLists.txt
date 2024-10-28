

set(SourceFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/defs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Dtype.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Frame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/File.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NumpyFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RawFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SubFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NumpyHelpers.cpp
)


add_library(aare_core STATIC ${SourceFiles})
target_include_directories(aare_core PUBLIC 
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)


    
target_link_libraries(aare_core PUBLIC fmt::fmt PRIVATE aare_compiler_flags nlohmann_json::nlohmann_json)

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
            ${CMAKE_CURRENT_SOURCE_DIR}/NumpyFile.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/NumpyHelpers.test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/RawFile.test.cpp
            # ${CMAKE_CURRENT_SOURCE_DIR}/test/CircularFifo.test.cpp
            # ${CMAKE_CURRENT_SOURCE_DIR}/test/wrappers.test.cpp
            # ${CMAKE_CURRENT_SOURCE_DIR}/test/Transforms.test.cpp

        )
    target_sources(tests PRIVATE ${TestSources} )
endif()