// Your First C++ Program
#include "aare/aare.hpp"
#include "aare/examples/defs.hpp"
#include <iostream>

using namespace aare;

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

    std::filesystem::path const path2("/tmp/raw_example_writing.json");
    aare::FileConfig config;
    config.version = "1.0";
    config.geometry = {1, 1};
    config.detector_type = aare::DetectorType::Moench;
    config.max_frames_per_file = 100;
    config.rows = 1;
    config.cols = 1;
    config.dtype = aare::DType::UINT16;
    File file2(path2, "w", config);
    Frame frame(1024, 512, 16);

    for (int i = 0; i < 1024; i++) {
        for (int j = 0; j < 512; j++) {
            frame.set(i, j, (uint16_t)(i + j));
        }
    }

    sls_detector_header header;
    header.frameNumber = 0;
    file2.write(frame, header);
}