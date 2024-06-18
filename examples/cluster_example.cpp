#include "aare.hpp"
#include "aare/examples/defs.hpp"

#include <cassert>
#include <iostream>

using namespace aare;
int main() {
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv("AARE_ROOT_DIR"));
    std::filesystem::path const fpath("/home/l_bechir/tmp/testNewFW20230714/clust/cu_half_speed_d0_f0_4.clust");
    ClusterFileV2 cf(fpath, "r");
    auto clusters = cf.read();
    for (auto &c : clusters) {
        std::cout << c.frame_number << std::endl;
    }

}