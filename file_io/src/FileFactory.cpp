#include "aare/FileFactory.hpp"
#include "aare/File.hpp"
#include "aare/JsonFileFactory.hpp"
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
        throw std::runtime_error("Numpy file not implemented");
    }

    throw std::runtime_error("Unsupported file type");
}

template <DetectorType detector, typename DataType>
void FileFactory<detector, DataType>::parse_fname(File<detector, DataType> *file) {
    file->base_path = fpath.parent_path();
    file->base_name = fpath.stem();
    file->ext = fpath.extension();

    auto pos = file->base_name.rfind("_");
    file->findex = std::stoi(file->base_name.substr(pos + 1));
    pos = file->base_name.find("_master_");
    file->base_name.erase(pos);
}

template <DetectorType detector, typename DataType>
sls_detector_header FileFactory<detector, DataType>::read_header(const std::filesystem::path &fname) {
    sls_detector_header h{};
    FILE *fp = fopen(fname.c_str(), "r");
    if (!fp)
        throw std::runtime_error(fmt::format("Could not open: {} for reading", fname.c_str()));

    size_t rc = fread(reinterpret_cast<char *>(&h), sizeof(h), 1, fp);
    fclose(fp);
    if (rc != 1)
        throw std::runtime_error("Could not read header from file");
    return h;
}

template <DetectorType detector, typename DataType>
void FileFactory<detector, DataType>::find_geometry(File<detector, DataType> *file) {
    uint16_t r{};
    uint16_t c{};
    for (int i = 0; i != file->n_subfiles; ++i) {
        auto h = this->read_header(file->data_fname(i, 0));
        r = std::max(r, h.row);
        c = std::max(c, h.column);

        file->positions.push_back({h.row, h.column});
    }
    r++;
    c++;

    file->rows = r * file->subfile_rows;
    file->cols = c * file->subfile_cols;

    file->rows += (r - 1) * file->cfg.module_gap_row;
}

template class FileFactory<DetectorType::Jungfrau, uint16_t>;
