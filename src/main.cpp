// Your First C++ Program
#include <iostream>
#include <FileHandler.hpp>
void test(FileHandler* f,int frame_number){
    std::cout << "frame number: " << frame_number << std::endl;
    Frame16* frame = f->get_frame<uint16_t>(frame_number);
    std::cout << frame->get(0,0) << ' ';
    std::cout << frame->get(0,1) << ' ';
    std::cout << frame->get(1,0) << ' ';
    std::cout << frame->get(511,1023) << std::endl;

    delete frame;
}

int main() {
    std::filesystem::path fpath("/home/l_bechir/github/aare/data/jungfrau_single_master_0.json");
    FileHandler* fileHandler = new FileHandler(fpath);
    test(fileHandler,0);

    delete fileHandler;

}
