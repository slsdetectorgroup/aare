
FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG  tags/v3.8.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(simdjson)
# hide simdjson warnings by making the includes system includes
get_target_property(_inc simdjson INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(simdjson SYSTEM INTERFACE ${_inc})


add_library(network_io STATIC 
    src/ZmqSocketReceiver.cpp
    src/ZmqSocketSender.cpp
    src/ZmqSocket.cpp
    src/ZmqHeader.cpp
    src/ZmqMultiReceiver.cpp
    src/ZmqVentilator.cpp
    src/ZmqWorker.cpp
    src/ZmqSink.cpp
    )



target_include_directories(network_io PUBLIC include)
target_link_libraries(network_io PRIVATE   libzmq fmt::fmt core utils   aare_compiler_flags )
target_link_libraries(network_io PUBLIC simdjson)

if(AARE_PYTHON_BINDINGS)
set_property(TARGET file_io PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

if(AARE_TESTS)
    set(TestSources 
    ${CMAKE_CURRENT_SOURCE_DIR}/test/ZmqHeader.test.cpp
    )
    target_sources(tests PRIVATE ${TestSources} )
    target_link_libraries(tests PRIVATE network_io core utils)
endif()