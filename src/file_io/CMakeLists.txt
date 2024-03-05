
set(FILE_IO_SOURCES "file/File.cpp"
                    "file/JsonFile.cpp"
                    "file/SubFile.cpp"
                    "file_factory/FileFactory.cpp"
                    "file_factory/JsonFileFactory.cpp"
                    "file_factory/RawFileFactory.cpp"
                    "helpers.cpp"
                    )
# append ${CMAKE_CURRENT_SOURCE_DIR} to the list of sources using for loop
foreach(FILE_IO_SOURCE ${FILE_IO_SOURCES})
    list(APPEND FILE_IO_SOURCES_WITH_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${FILE_IO_SOURCE}")
endforeach()

add_library(file_io ${FILE_IO_SOURCES_WITH_PATH})


target_include_directories(file_io PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/file")
target_include_directories(file_io PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/file_factory")
target_include_directories(file_io PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")


target_link_libraries(file_io common fmt::fmt core nlohmann_json::nlohmann_json)