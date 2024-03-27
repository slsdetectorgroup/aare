#include "aare/FileFactory.hpp"
#include "aare/FileInterface.hpp"
#include "aare/RawFileFactory.hpp"
#include "aare/NumpyFileFactory.hpp"
#include "aare/utils/logger.hpp"
#include "aare/utils/logger.hpp"
#include <iostream>

FileFactory *FileFactory::get_factory(std::filesystem::path fpath) {
    // check if file exists
    if (!std::filesystem::exists(fpath)) {
        throw std::runtime_error("File does not exist");
    }

    if (fpath.extension() == ".raw" || fpath.extension() == ".json"){
        aare::logger::info("Loading",fpath.extension(),"file");
        return new RawFileFactory(fpath);
    } 
    if (fpath.extension() == ".raw" || fpath.extension() == ".json"){
        aare::logger::info("Loading",fpath.extension(),"file");
        return new RawFileFactory(fpath);
    } 
    // check if extension is numpy
    // else if (fpath.extension() == ".npy") {
    //     aare::logger::info("Loading numpy file");
    //     return new NumpyFileFactory(fpath);
    // }

    throw std::runtime_error("Unsupported file type");
}



