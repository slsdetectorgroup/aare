
FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG  tags/v3.8.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(simdjson)



add_library(network_io STATIC 
    src/ZmqSocketReceiver.cpp
    src/ZmqSocketSender.cpp
    src/ZmqSocket.cpp
    src/ZmqHeader.cpp
    )



target_include_directories(network_io PUBLIC include)
target_link_libraries(network_io PRIVATE simdjson  libzmq fmt::fmt core utils  aare_compiler_flags )
# target_link_libraries(network_io LINK_PRIVATE  )

if(AARE_PYTHON_BINDINGS)
set_property(TARGET file_io PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

# if(AARE_TESTS)
#     set(TestSources
#             ${CMAKE_CURRENT_SOURCE_DIR}/test/NumpyFile.test.cpp
#         )
#     target_sources(tests PRIVATE ${TestSources} )
#     target_link_libraries(tests PRIVATE core network_io)
# endif()
