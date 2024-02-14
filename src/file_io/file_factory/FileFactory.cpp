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
