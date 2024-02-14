target_include_directories(aare PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/file")
target_include_directories(aare PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/file_factory")
target_include_directories(aare PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

set(FILE_IO_SOURCES "file/File.cpp"
                    "file/JsonFile.cpp"
                    "file_factory/FileFactory.cpp"
                    "file_factory/JsonFileFactory.cpp"
                    "file_factory/RawFileFactory.cpp"
                    "helpers.cpp"
                    )
# append ${CMAKE_CURRENT_SOURCE_DIR} to the list of sources using for loop
foreach(FILE_IO_SOURCE ${FILE_IO_SOURCES})
    list(APPEND FILE_IO_SOURCES_WITH_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${FILE_IO_SOURCE}")
endforeach()

target_sources(aare PRIVATE ${FILE_IO_SOURCES_WITH_PATH})