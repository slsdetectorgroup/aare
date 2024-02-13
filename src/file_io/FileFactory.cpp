#include "FileFactory.hpp"
#include "File.hpp"
#include "RawFileFactory.hpp"
#include "JsonFileFactory.hpp"
#include <iostream>

File FileFactory::loadFile(std::filesystem::path fpath){
    // check if file exists
    if(!std::filesystem::exists(fpath)){
        throw std::runtime_error("File does not exist");
    }

    File ret;
    // check if extension is raw
    if(fpath.extension() == ".raw"){
        std::cout<<"Loading raw file"<<std::endl;
    }
    // json
    else if(fpath.extension() == ".json"){
        std::cout<<"Loading json file"<<std::endl;
        JsonFileFactory jff = JsonFileFactory();
        

    }
    //check if extension is numpy
    else if(fpath.extension() == ".npy"){
        std::cout<<"Loading numpy file"<<std::endl;
    }
    // TODO: determine file type
    return ret ;
}