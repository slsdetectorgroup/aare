add_executable(json_example "${CMAKE_CURRENT_SOURCE_DIR}/json.cpp")
target_link_libraries(json_example PRIVATE aare_compiler_flags)

add_executable(numpy_example "${CMAKE_CURRENT_SOURCE_DIR}/numpy.cpp")
target_link_libraries(numpy_example PRIVATE aare_compiler_flags)


target_link_libraries(json_example PUBLIC aare)
target_link_libraries(numpy_example PUBLIC aare)
