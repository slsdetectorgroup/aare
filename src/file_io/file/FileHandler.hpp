#include <filesystem>
#include "FileFactory.hpp"
#include "File.hpp"
class FileHandler{
    private:
    std::filesystem::path fpath;
    FileFactory* fileFactory;
    File* f;

    public:
    FileHandler(std::filesystem::path fpath){
        this->fpath = fpath;
         this->fileFactory= FileFactory::get_factory(fpath);
         this->f= fileFactory->load_file();
         delete fileFactory;
    }

    template <typename DataType>
    Frame<DataType>* get_frame(int index){
         FrameImpl* frame =f->get_frame(index);
         return  static_cast<Frame<DataType>*>(frame);
    }

    

    ~FileHandler(){
        delete f;
    }
};