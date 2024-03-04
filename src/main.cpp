// Your First C++ Program
#include <iostream>
#include <FileHandler.hpp>

using JFileHandler = FileHandler<DetectorType::Jungfrau,uint16_t>;
using JFile = File<DetectorType::Jungfrau,uint16_t>;
using JFrame = Frame<uint16_t>;


void test(JFileHandler* f,int frame_number){
    std::cout << "frame number: " << frame_number << std::endl;
    JFrame* frame = f->get_frame(frame_number);
    std::cout << frame->get(0,0) << ' ';
    std::cout << frame->get(0,1) << ' ';
    std::cout << frame->get(1,0) << ' ';
    std::cout << frame->get(511,1023) << std::endl;

    delete frame;
}

int main() {
    std::filesystem::path fpath("/home/l_bechir/github/aare/data/jungfrau_single_master_0.json");
    auto fileHandler = new JFileHandler (fpath);

    delete fileHandler;

}
