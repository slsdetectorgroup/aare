#include <filesystem>
#include "aare/FileFactory.hpp"
#include "aare/File.hpp"

class FileHandler{
    private:
    std::filesystem::path fpath;
    FileFactory* fileFactory;
    File* f;

    public:
    FileHandler(std::filesystem::path fname){
        this->fpath = fname;
         this->fileFactory= FileFactory::get_factory(fname);
         this->f= fileFactory->load_file();
         delete fileFactory;
    }

    Frame* get_frame(int index){
         return  f->get_frame(index);
    }

    

    ~FileHandler(){
        delete f;
    }
};