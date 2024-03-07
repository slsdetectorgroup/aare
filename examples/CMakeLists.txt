add_executable(example "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

target_link_libraries(example PUBLIC aare aare_compiler_flags)


target_include_directories(example PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")



