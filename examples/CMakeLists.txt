
set(EXAMPLE_LIST "json_example;logger_example;numpy_read_example;multiport_example;raw_example;")
set(EXAMPLE_LIST "${EXAMPLE_LIST};mythen_example;numpy_write_example;zmq_sender_example;")    
set(EXAMPLE_LIST "${EXAMPLE_LIST};cluster_example;zmq_multi_receiver;zmq_worker;zmq_sink")
set(EXAMPLE_LIST "${EXAMPLE_LIST};zmq_task_ventilator;zmq_restream_example;zmq_receiver_example")
set(EXAMPLE_LIST "${EXAMPLE_LIST};testing_pedestal;cluster_finder_example;reorder_moench_example")

foreach(example ${EXAMPLE_LIST})
    add_executable(${example} ${example}.cpp)
    target_include_directories(${example} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(${example} PUBLIC aare PRIVATE aare_compiler_flags libzmq )
endforeach()





