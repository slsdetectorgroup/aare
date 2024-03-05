# target_include_directories(aare PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# target_sources(aare PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Frame.cpp")


add_library(core "${CMAKE_CURRENT_SOURCE_DIR}/Frame.cpp")

target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(core PUBLIC common)


