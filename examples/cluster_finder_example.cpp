
#include "aare.hpp"
#include "aare/examples/defs.hpp"

#include <cassert>
#include <iostream>

using namespace aare;
int main() {
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv("AARE_ROOT_DIR"));
    std::filesystem::path const fpath("/mnt/sls_det_storage/moench_data/testNewFW20230714/cu_half_speed_master_4.json");
    auto file = RawFile(fpath, "r");

    ClusterFinder clusterFinder(3, 3, 5, 1);
    Pedestal p(file.rows(), file.cols());
    ClusterFile cf("/tmp/cu_half_speed_4.clust", "w", ClusterFileConfig{1, 2, 3, 3, DType::INT16});

    logger::info("file rows:", file.rows(), "cols:", file.cols());
    logger::info(file.total_frames());

    for (size_t i = 0; i < file.total_frames(); i++) {

        auto frame = file.read();
        auto clusters = clusterFinder.find_clusters(frame.view<int16_t>(), p);
        cf.write(clusters);
        logger::info("frame:", i, "nclusters:", clusters.size());
    }

    // auto clusters = clusterFinder.find_clusters(frame.span(), p);

    // aare::logger::info("nclusters:", clusters.size());

    // for (auto &cluster : clusters) {
    //     aare::logger::info("cluster center:", cluster.to_string<double>());
    // }
}