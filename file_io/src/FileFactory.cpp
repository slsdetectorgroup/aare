#include "aare/FileFactory.hpp"
#include "aare/File.hpp"
#include "aare/JsonFileFactory.hpp"
#include "aare/NumpyFileFactory.hpp"
#include <iostream>

FileFactory *FileFactory::get_factory(std::filesystem::path fpath) {
    // check if file exists
    if (!std::filesystem::exists(fpath)) {
        throw std::runtime_error("File does not exist");
    }

    if (fpath.extension() == ".raw") {
        std::cout << "Loading raw file" << std::endl;
        throw std::runtime_error("Raw file not implemented");
    } else if (fpath.extension() == ".json") {
        std::cout << "Loading json file" << std::endl;
        return new JsonFileFactory(fpath);
    }
    // check if extension is numpy
    else if (fpath.extension() == ".npy") {
        std::cout << "Loading numpy file" << std::endl;
        return new NumpyFileFactory(fpath);
    }

    throw std::runtime_error("Unsupported file type");
}



