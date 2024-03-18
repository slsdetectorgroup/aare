// Your First C++ Program
#include "aare/DataSpan.hpp"
#include "aare/Frame.hpp"
#include "aare/ImageData.hpp"
#include "aare/IFrame.hpp"
#include "aare/FileHandler.hpp"
#include <iostream>

#define AARE_ROOT_DIR_VAR "PROJECT_ROOT_DIR"

using JFileHandler = FileHandler<DetectorType::Jungfrau, uint16_t>;
using JFile = File<DetectorType::Jungfrau, uint16_t>;
using JFrame = Frame<uint16_t>;

void test(JFileHandler *f, int frame_number) {
    std::cout << "frame number: " << frame_number << std::endl;
    JFrame *frame = f->get_frame(frame_number);
    
    DataSpan<uint16_t> span(*frame);
    ImageData<uint16_t> img(span);

    std::cout << frame->get(0, 0) << std::endl;
    std::cout << frame->get(0, 1) << std::endl;
    std::cout << frame->get(1, 0) << std::endl;
    std::cout << frame->get(49, 49) << std::endl;

    delete frame;
}

int main() {
    auto  PROJECT_ROOT_DIR = std::filesystem::path(getenv(AARE_ROOT_DIR_VAR));
    std::filesystem::path fpath(PROJECT_ROOT_DIR / "data" / "jungfrau_single_master_0.json");
    // std::filesystem::path fpath(PROJECT_ROOT_DIR / "data" / "test_numpy_file.npy");
        std::cout<<fpath<<std::endl;


    auto fileHandler = new JFileHandler(fpath);
    test(fileHandler, 0);
    test(fileHandler, 9);

    delete fileHandler;

}
