
set(EXAMPLE_LIST "json_example;logger_example;numpy_read_example;multiport_example;raw_example;zmq_restream_example")
set(EXAMPLE_LIST "${EXAMPLE_LIST};mythen_example;numpy_write_example;zmq_receiver_example;zmq_sender_example;")    
set(EXAMPLE_LIST "${EXAMPLE_LIST};cluster_example;zmq_multi_receiver;task_ventilator;worker;sink")
foreach(example ${EXAMPLE_LIST})
    add_executable(${example} ${example}.cpp)
    target_include_directories(${example} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(${example} PUBLIC aare PRIVATE aare_compiler_flags)
endforeach()


message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")



