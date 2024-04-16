// Your First C++ Program
#include "aare/examples/defs.hpp"
#include "aare/file_io/File.hpp"
#include "aare/utils/logger.hpp"
#include <iostream>

using aare::File;
using aare::Frame;

void test(File &f, int frame_number) {
    std::cout << "frame number: " << frame_number << '\n';
    Frame frame = f.iread(frame_number);
    std::cout << *(reinterpret_cast<uint16_t *>(frame.get(0, 0))) << '\n';
    std::cout << *(reinterpret_cast<uint16_t *>(frame.get(0, 1))) << '\n';
    std::cout << *(reinterpret_cast<uint16_t *>(frame.get(0, 95))) << '\n';
}

int main() {
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv(AARE_ROOT_DIR));
    if (PROJECT_ROOT_DIR.empty()) {
        throw std::runtime_error("environment variable PROJECT_ROOT_DIR is not set");
    }
    std::filesystem::path const fpath(PROJECT_ROOT_DIR / "data" / "moench" /
                                      "moench04_noise_200V_sto_both_100us_no_light_thresh_900_master_0.raw");
    File file(fpath, "r");
    test(file, 0);
    test(file, 2);
    test(file, 99);
}