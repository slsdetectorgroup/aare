#include "aare.hpp"
#include "aare/examples/defs.hpp"

#include <iostream>

using aare::File;
using aare::Frame;

void test(File &f, int frame_number) {
    std::cout << "frame number: " << frame_number << '\n';
    Frame frame = f.iread(frame_number);
    std::cout << *(reinterpret_cast<uint16_t *>(frame.get(0, 0))) << '\n';
    std::cout << *(reinterpret_cast<uint16_t *>(frame.get(0, 1))) << '\n';
    std::cout << *(reinterpret_cast<uint16_t *>(frame.get(1, 0))) << '\n';
    std::cout << *(reinterpret_cast<uint16_t *>(frame.get(49, 49))) << '\n';
}

int main() {

    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv(AARE_ROOT_DIR));
    std::filesystem::path const fpath(PROJECT_ROOT_DIR / "data" / "numpy" / "test_numpy_file.npy");
    std::cout << fpath << '\n';

    File file(fpath, "r");
    test(file, 0);
    test(file, 2);
    test(file, 24);
}
