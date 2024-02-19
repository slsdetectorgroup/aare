// Your First C++ Program
#include <iostream>
#include "file_io/file_factory/FileFactory.hpp"


int main() {
    FileFactory fileFactory=FileFactory::getFactory(std::filesystem::path("/home/l_bechir/github/aare")/"data"/"m3_master_0.json");
    std::cout<<"filefactory is of instance: "<<typeid(fileFactory).name()<<std::endl;
    File f = fileFactory.loadFile();
}
