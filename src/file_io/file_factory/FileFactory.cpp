#include "FileFactory.hpp"
#include "File.hpp"
#include "RawFileFactory.hpp"
#include "JsonFileFactory.hpp"
#include <iostream>



FileFactory FileFactory::getFactory(std::filesystem::path fpath){
    // check if file exists
    if(!std::filesystem::exists(fpath)){
        throw std::runtime_error("File does not exist");
    }

    if(fpath.extension() == ".raw"){
        std::cout<<"Loading raw file"<<std::endl;
        throw std::runtime_error("Raw file not implemented");
    }
    else if(fpath.extension() == ".json"){
        std::cout<<"Loading json file"<<std::endl;
        return JsonFileFactory(fpath);
    }
    //check if extension is numpy
    else if(fpath.extension() == ".npy"){
        std::cout<<"Loading numpy file"<<std::endl;
        throw std::runtime_error("Numpy file not implemented");
    }
    
    throw std::runtime_error("Unsupported file type");    
}

void FileFactory::parse_fname(File& file) {
    file.base_path = fpath.parent_path();
    file.base_name = fpath.stem();
    file.ext = fpath.extension();

    auto pos = file.base_name.rfind("_");
    file.findex = std::stoi(file.base_name.substr(pos + 1));
    pos = file.base_name.find("_master_");
    file.base_name.erase(pos);
}

template <typename Header=sls_detector_header>
 Header FileFactory::read_header(const std::filesystem::path &fname) {
    Header h{};
    FILE *fp = fopen(fname.c_str(), "r");
    if (!fp)
        throw std::runtime_error(
            fmt::format("Could not open: {} for reading", fname.c_str()));

    size_t rc = fread(reinterpret_cast<char *>(&h), sizeof(h), 1, fp);
    fclose(fp);
    if (rc != 1)
        throw std::runtime_error("Could not read header from file");
    return h;
   
}

void FileFactory::find_geometry(File& file) {
    uint16_t r{};
    uint16_t c{};
    for (int i = 0; i != file.n_subfiles; ++i) {
        auto h = this->read_header(file.data_fname(i, 0));
        r = std::max(r, h.row);
        c = std::max(c, h.column);

        file.positions.push_back({h.row, h.column});
    }
    r++;
    c++;

    file.rows = r * file.subfile_rows;
    file.cols = c * file.subfile_cols;

    file.rows += (r - 1) * cfg.module_gap_row;
}