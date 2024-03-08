// Your First C++ Program
#include "aare/FileHandler.hpp"
#include <iostream>

using JFileHandler = FileHandler<DetectorType::Jungfrau, uint16_t>;
using JFile = File<DetectorType::Jungfrau, uint16_t>;
using JFrame = Frame<uint16_t>;

void test(JFileHandler *f, int frame_number) {
    std::cout << "frame number: " << frame_number << std::endl;
    JFrame *frame = f->get_frame(frame_number);
    std::cout << frame->get(0, 0) << std::endl;
    std::cout << frame->get(0, 1) << std::endl;
    std::cout << frame->get(1, 0) << std::endl;
    std::cout << frame->get(49, 49) << std::endl;

    delete frame;
}

int main() {
    // std::filesystem::path fpath("/home/bb/github/aare/data/jungfrau_single_master_0.json");
    std::filesystem::path fpath("/home/bb/github/aare/data/test_numpy_file.npy");

    auto fileHandler = new JFileHandler(fpath);
    test(fileHandler, 0);
    test(fileHandler, 24);

    delete fileHandler;

}
