#pragma once
#include <filesystem>
#include "File.hpp"

class FileFactory{
    // Class that will be used to create File objects
    // follows the factory pattern
public:
    // virtual File createFile() = 0;
    // virtual int deleteFile() = 0;
    File loadFile(std::filesystem::path); //TODO: add option to load all file to memory or keep it on disk
};
