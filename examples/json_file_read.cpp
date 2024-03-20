// Your First C++ Program
#include "aare/FileHandler.hpp"
#include <iostream>

#define AARE_ROOT_DIR_VAR "PROJECT_ROOT_DIR"

void test(FileHandler *f, int frame_number) {
    std::cout << "frame number: " << frame_number << std::endl;
    Frame *frame = f->get_frame(frame_number);
    std::cout << *((uint16_t *)frame->get(0, 0)) << std::endl;
    std::cout << *((uint16_t *)frame->get(0, 1)) << std::endl;
    std::cout << *((uint16_t *)frame->get(1, 0)) << std::endl;
    std::cout << *((uint16_t *)frame->get(511, 1023)) << std::endl;

    delete frame;
}

int main() {
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv(AARE_ROOT_DIR_VAR));
    std::filesystem::path fpath(PROJECT_ROOT_DIR / "data" / "jungfrau_single_master_0.json");
    // std::filesystem::path fpath(PROJECT_ROOT_DIR / "data" / "test_numpy_file.npy");
    std::cout << fpath << std::endl;

    auto fileHandler = new FileHandler(fpath);
    test(fileHandler, 0);
    test(fileHandler, 2);
    test(fileHandler, 9);

    delete fileHandler;
}
