#pragma once
#include <filesystem>
#include "File.hpp"

class FileFactory{
    // Class that will be used to create File objects
    // follows the factory pattern
    protected:
    std::filesystem::path fpath;
public:
    static FileFactory getFactory(std::filesystem::path);
    // virtual int deleteFile() = 0;
    virtual File loadFile(){};//TODO: add option to load all file to memory or keep it on disk
    virtual void parse_metadata(File&){};

    // inline fs::path master_fname() const {
    // return base_path / fmt::format("{}_master_{}{}", base_name, findex, ext);}

    void parse_fname(File&);

};
