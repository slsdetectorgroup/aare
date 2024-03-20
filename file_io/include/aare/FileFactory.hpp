#pragma once
#include <filesystem>
#include "aare/File.hpp"
class FileFactory{
    // Class that will be used to create File objects
    // follows the factory pattern
    protected:
    std::filesystem::path m_fpath;
public:
    static FileFactory* get_factory(std::filesystem::path);
    // virtual int deleteFile() = 0;
    virtual File* load_file()=0;//TODO: add option to load all file to memory or keep it on disk
    virtual void parse_metadata(File*)=0;
    virtual void parse_fname(File*)=0;
    virtual ~FileFactory() = default;




};
