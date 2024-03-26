// Your First C++ Program
#include "aare/ContextManager.hpp"
#include <iostream>
#include "aare/utils/logger.hpp"

#define AARE_ROOT_DIR_VAR "PROJECT_ROOT_DIR"

void test(File *f, int frame_number) {
    std::cout << "frame number: " << frame_number << std::endl;
    Frame frame = f->get_frame(frame_number);
    std::cout << *((uint16_t *)frame.get(0, 0)) << std::endl;
    std::cout << *((uint16_t *)frame.get(0,1)) << std::endl;
    std::cout << *((uint16_t *)frame.get(255, 1023)) << std::endl;
    std::cout << *((uint16_t *)frame.get(511, 1023)) << std::endl;



}

int main() {
    ContextManager ctx_manager;
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv(AARE_ROOT_DIR_VAR));
    std::filesystem::path fpath(PROJECT_ROOT_DIR / "data" / "jungfrau_double_master_0.json");
    std::cout << fpath << std::endl;


    File *file = ctx_manager.get_file(fpath);
    test(file, 0);
    test(file, 9);




}