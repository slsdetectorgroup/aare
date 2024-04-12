// Your First C++ Program
#include "aare/file_io/File.hpp"
#include "aare/utils/logger.hpp"
#include <iostream>

#define AARE_ROOT_DIR_VAR "PROJECT_ROOT_DIR"

using aare::File;
using aare::Frame;

void test1(File &f, int frame_number) {
    std::cout << "frame number: " << frame_number << '\n';
    Frame frame = f.iread(frame_number);
    std::cout << *(reinterpret_cast<uint32_t *>(frame.get(0, 0))) << '\n';
    std::cout << *(reinterpret_cast<uint32_t *>(frame.get(0, 1))) << '\n';
    std::cout << *(reinterpret_cast<uint32_t *>(frame.get(0, 3839))) << '\n';

    for (int i = 0; i < 3840; i++) {
        uint16_t const x = *(reinterpret_cast<uint32_t *>(frame.get(0, i)));
        if (x != i) {
            aare::logger::error("error at i", i, "x", x);
        }
    }
}

void test2(File &f, int frame_number) {
    std::cout << "frame number: " << frame_number << '\n';
    Frame frame = f.iread(frame_number);
    std::cout << *(reinterpret_cast<uint32_t *>(frame.get(0, 0))) << '\n';
    std::cout << *(reinterpret_cast<uint32_t *>(frame.get(0, 1))) << '\n';
    std::cout << *(reinterpret_cast<uint32_t *>(frame.get(0, 1280 * 4 - 1))) << '\n';
}

int main() {
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv(AARE_ROOT_DIR_VAR));
    if (PROJECT_ROOT_DIR.empty()) {
        throw std::runtime_error("environment variable PROJECT_ROOT_DIR is not set");
    }
    std::filesystem::path fpath(PROJECT_ROOT_DIR / "data" / "mythen" / "m3_master_0.json");
    File file(fpath, "r");
    test1(file, 0);

    fpath = (PROJECT_ROOT_DIR / "data" / "mythen" / "scan242_master_3.raw");
    File file2(fpath, "r");
    test2(file2, 0);
}