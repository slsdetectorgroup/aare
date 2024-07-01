#include "aare.hpp"
#include "aare/examples/defs.hpp"

#include <iostream>

using namespace aare;


int main() {
    std::filesystem::path const fpath("/mnt/sls_det_storage/moench_data/testNewFW20230714/cu_half_speed_master_4.json");
    File file(fpath, "r");

    Frame frame = file.iread(0);
    std::cout << frame.rows() << " " << frame.cols() << " " << frame.bitdepth() << std::endl;
    Transforms transforms;
    transforms.add(Transforms::reorder_moench());
    transforms(frame);
}