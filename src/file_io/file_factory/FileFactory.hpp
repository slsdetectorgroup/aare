#pragma once
#include <filesystem>
#include "File.hpp"

class FileFactory{
    // Class that will be used to create File objects
    // follows the factory pattern
    protected:
    std::filesystem::path fpath;
public:
    static FileFactory* getFactory(std::filesystem::path);
    // virtual int deleteFile() = 0;
    virtual File* loadFile()=0;//TODO: add option to load all file to memory or keep it on disk
    virtual void parse_metadata(File*)=0;
    
    
    void find_geometry(File*);
    void parse_fname(File*);

    template <typename Header> Header read_header(const std::filesystem::path &fname);



};
