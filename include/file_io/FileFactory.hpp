#pragma once
#include <filesystem>
#include "file_io/File.hpp"
template <DetectorType detector,typename DataType>
class FileFactory{
    // Class that will be used to create File objects
    // follows the factory pattern
    protected:
    std::filesystem::path fpath;
public:
    static FileFactory<detector,DataType>* get_factory(std::filesystem::path);
    // virtual int deleteFile() = 0;
    virtual File<detector,DataType>* load_file()=0;//TODO: add option to load all file to memory or keep it on disk
    virtual void parse_metadata(File<detector,DataType>*)=0;
    
    
    void find_geometry(File<detector,DataType>*);
    void parse_fname(File<detector,DataType>*);

    sls_detector_header read_header(const std::filesystem::path &fname);



};

