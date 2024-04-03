// Your First C++ Program
#include "aare/File.hpp"
#include "aare/utils/logger.hpp"
#include <iostream>

#define AARE_ROOT_DIR_VAR "PROJECT_ROOT_DIR"

using aare::File;
using aare::Frame;

void test(File &f, int frame_number) {
    std::cout << "frame number: " << frame_number << std::endl;
    Frame frame = f.iread(frame_number);
    std::cout << *((uint16_t *)frame.get(0, 0)) << std::endl;
    std::cout << *((uint16_t *)frame.get(0, 1)) << std::endl;
    std::cout << *((uint16_t *)frame.get(1, 0)) << std::endl;
    std::cout << *((uint16_t *)frame.get(511, 1023)) << std::endl;
}

int main() {
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv(AARE_ROOT_DIR_VAR));
    std::filesystem::path fpath(PROJECT_ROOT_DIR / "data" / "jungfrau" / "jungfrau_single_master_0.json");
    std::cout << fpath << std::endl;

    File file(fpath, "r");
    test(file, 0);
    test(file, 2);
    test(file, 9);
}