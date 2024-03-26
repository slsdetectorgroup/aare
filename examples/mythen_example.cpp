// Your First C++ Program
#include "aare/ContextManager.hpp"
#include "aare/utils/logger.hpp"
#include <iostream>

#define AARE_ROOT_DIR_VAR "PROJECT_ROOT_DIR"

void test(File *f, int frame_number) {
    std::cout << "frame number: " << frame_number << std::endl;
    Frame frame = f->get_frame(frame_number);
    std::cout << *((uint16_t *)frame.get(0, 0)) << std::endl;
    std::cout << *((uint16_t *)frame.get(0, 1)) << std::endl;
    std::cout << *((uint16_t *)frame.get(0, 3839)) << std::endl;

    for (int i = 0; i < 3840; i++) {
        uint16_t x = *((uint16_t *)frame.get(0, i));
        if (x != i) {
            aare::logger::error("error at i", i, "x", x);
        }
    }
}

int main() {
    ContextManager ctx_manager;
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv(AARE_ROOT_DIR_VAR));
    if (PROJECT_ROOT_DIR.empty()) {
        throw std::runtime_error("environment variable PROJECT_ROOT_DIR is not set");
    }
    std::filesystem::path fpath(PROJECT_ROOT_DIR / "data" / "mythen" / "m3_master_0.json");
    File *file = ctx_manager.get_file(fpath);
    test(file, 0);
}