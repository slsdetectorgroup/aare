
set(EXAMPLE_LIST "json_example;logger_example;numpy_read_example;multiport_example;raw_example;")
set(EXAMPLE_LIST "${EXAMPLE_LIST};mythen_example;numpy_write_example;zmq_receiver_example;zmq_sender_example;")
foreach(example ${EXAMPLE_LIST})
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PUBLIC aare PRIVATE aare_compiler_flags)


endforeach()





