#include <filesystem>
#include "aare/FileFactory.hpp"
#include "aare/File.hpp"

template <DetectorType detector,typename DataType>
class FileHandler{
    private:
    std::filesystem::path fpath;
    FileFactory<detector,DataType>* fileFactory;
    File<detector,DataType>* f;

    public:
    FileHandler<detector,DataType>(std::filesystem::path fname){
        this->fpath = fname;
         this->fileFactory= FileFactory<detector,DataType>::get_factory(fname);
         this->f= fileFactory->load_file();
         delete fileFactory;
    }

    Frame<DataType>* get_frame(int index){
         return  f->get_frame(index);
    }

    

    ~FileHandler(){
        delete f;
    }
};