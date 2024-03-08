

set(SourceFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/File.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/helpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/JsonFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/JsonFileFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SubFile.cpp
)

add_library(file_io STATIC ${SourceFiles})
target_include_directories(file_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(file_io  PUBLIC fmt::fmt core nlohmann_json::nlohmann_json)
set_property(TARGET file_io PROPERTY POSITION_INDEPENDENT_CODE ON)


# if(AARE_BUILD_TESTS)
#     set(TestSources
#             ${CMAKE_CURRENT_SOURCE_DIR}/src/defs.test.cpp
#         )
#     target_sources(tests PRIVATE ${TestSources} )

#     #Work around to remove, this is not the way to do it =) 
#     # target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/include/common)
#     target_link_libraries(tests PRIVATE file_io)

# endif()
