// Your First C++ Program
#include "aare/file_io/File.hpp"
#include "aare/utils/logger.hpp"
#include <iostream>

#define AARE_ROOT_DIR_VAR "PROJECT_ROOT_DIR"

using aare::File;
using aare::Frame;

void test(File &f, int frame_number) {
    std::cout << "frame number: " << frame_number << '\n';
    Frame frame = f.iread(frame_number);
    std::cout << *(reinterpret_cast<uint16_t *>(frame.get(0, 0))) << '\n';
    std::cout << *(reinterpret_cast<uint16_t *>(frame.get(0, 1))) << '\n';
    std::cout << *(reinterpret_cast<uint16_t *>(frame.get(0, 1))) << '\n';
    std::cout << *(reinterpret_cast<uint16_t *>(frame.get(255, 1023))) << '\n';
    std::cout << *(reinterpret_cast<uint16_t *>(frame.get(511, 1023))) << '\n';
}

int main() {
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv(AARE_ROOT_DIR_VAR));
    std::filesystem::path const fpath(PROJECT_ROOT_DIR / "data" / "jungfrau" / "jungfrau_double_master_0.json");
    std::cout << fpath << '\n';

    File file(fpath, "r");
    test(file, 0);
    test(file, 9);
}