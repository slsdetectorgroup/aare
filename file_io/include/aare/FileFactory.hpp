#pragma once
#include <filesystem>
#include "aare/FileInterface.hpp"
class FileFactory{
    // Class that will be used to create FileInterface objects
    // follows the factory pattern
    protected:
    std::filesystem::path m_fpath;
public:
    static FileFactory* get_factory(std::filesystem::path);
    // virtual int deleteFile() = 0;
    static FileInterface* load_file(std::filesystem::path p){
        auto factory = get_factory(p);
        FileInterface* tmp= factory->load_file();
        delete factory;
        return tmp;
    };
    virtual FileInterface* load_file()=0;//TODO: add option to load all file to memory or keep it on disk
    virtual void parse_metadata(FileInterface*)=0;
    virtual void parse_fname(FileInterface*)=0;
    virtual ~FileFactory() = default;




};
