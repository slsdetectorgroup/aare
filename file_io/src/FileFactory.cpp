#include "aare/FileFactory.hpp"
#include "aare/File.hpp"
#include "aare/JsonFileFactory.hpp"
#include "aare/NumpyFileFactory.hpp"
#include <iostream>

template <DetectorType detector, typename DataType>
FileFactory<detector, DataType> *FileFactory<detector, DataType>::get_factory(std::filesystem::path fpath) {
    // check if file exists
    if (!std::filesystem::exists(fpath)) {
        throw std::runtime_error("File does not exist");
    }

    if (fpath.extension() == ".raw") {
        std::cout << "Loading raw file" << std::endl;
        throw std::runtime_error("Raw file not implemented");
    } else if (fpath.extension() == ".json") {
        std::cout << "Loading json file" << std::endl;
        return new JsonFileFactory<detector, DataType>(fpath);
    }
    // check if extension is numpy
    else if (fpath.extension() == ".npy") {
        std::cout << "Loading numpy file" << std::endl;
        return new NumpyFileFactory<detector, DataType>(fpath);
    }

    throw std::runtime_error("Unsupported file type");
}



template class FileFactory<DetectorType::Jungfrau, uint16_t>;