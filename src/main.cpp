// Your First C++ Program
#include <iostream>
#include "file_io/file_factory/FileFactory.hpp"

void test(File* f,int frame_number){
    std::cout << "frame number: " << frame_number << std::endl;
    auto frame = f->get_frame(frame_number);
    std::cout << frame.get(0,0) << ' ';
    std::cout << frame.get(0,1) << ' ';
    std::cout << frame.get(1,0) << ' ';
    std::cout << frame.get(511,1023) << std::endl;


}

int main() {
    FileFactory* fileFactory=FileFactory::getFactory(std::filesystem::path("/home/l_bechir/github/aare")/"data"/"jungfrau_single_master_0.json");
    File* f = fileFactory->loadFile();

    test(f,0);
    test(f,1);
    test(f,2);
    test(f,3);
    test(f,4);
    test(f,5);
    test(f,6);
    test(f,7);
    test(f,8);
    test(f,9);





    delete fileFactory;
}
