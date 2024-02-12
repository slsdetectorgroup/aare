add_subdirectory(file_io)
add_subdirectory(models)
add_subdirectory(processing)

target_include_directories(aare PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(aare PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)