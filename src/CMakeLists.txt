add_subdirectory(file_io)
add_subdirectory(core)
add_subdirectory(processing)
add_subdirectory(common)

target_include_directories(aare PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(aare PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
