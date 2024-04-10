#include "aare/file_io/FileFactory.hpp"
#include "aare/file_io/FileInterface.hpp"
#include "aare/file_io/NumpyFileFactory.hpp"
#include "aare/file_io/RawFileFactory.hpp"
#include "aare/utils/logger.hpp"
#include <iostream>

namespace aare {

FileFactory *FileFactory::get_factory(std::filesystem::path fpath) {
    if (fpath.extension() == ".raw" || fpath.extension() == ".json") {
        aare::logger::debug("Loading", fpath.extension(), "file");
        return new RawFileFactory(fpath);
    }
    if (fpath.extension() == ".raw" || fpath.extension() == ".json") {
        aare::logger::debug("Loading", fpath.extension(), "file");
        return new RawFileFactory(fpath);
    }
    // check if extension is numpy
    else if (fpath.extension() == ".npy") {
        aare::logger::debug("Loading numpy file");
        return new aare::NumpyFileFactory(fpath);
    }

    throw std::runtime_error("Unsupported file type");
}

} // namespace aare
