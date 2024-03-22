add_executable(json_example json_file_read.cpp)
add_executable(numpy_example numpy_file_read.cpp)


target_link_libraries(json_example PRIVATE aare_compiler_flags)
target_link_libraries(numpy_example PRIVATE aare_compiler_flags)

target_link_libraries(json_example PUBLIC aare)
target_link_libraries(numpy_example PUBLIC aare)



