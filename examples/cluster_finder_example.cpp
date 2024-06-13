
#include "aare.hpp"
#include "aare/examples/defs.hpp"

#include <cassert>
#include <iostream>

using namespace aare;
int main() {
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv("AARE_ROOT_DIR"));
    std::filesystem::path const fpath_frame(
        "/mnt/sls_det_storage/moench_data/testNewFW20230714/cu_half_speed_master_4.json");
    std::filesystem::path const fpath_cluster(
        "/mnt/sls_det_storage/moench_data/testNewFW20230714/cu_half_speed_d0_f0_4.clust");
    auto file = RawFile(fpath_frame, "r");

    // ClusterFinder clusterFinder(3, 3, 5, 1);
    // Pedestal p(file.rows(), file.cols());
    ClusterFileV2 cf("/tmp/cu_half_speed_4.clust", "r");
    auto clusters = cf.read();

    logger::info("RAW file");
    logger::info("file rows:", file.rows(), "cols:", file.cols());
    logger::info(file.total_frames());
    for (auto i = 0; i < file.total_frames(); i++) {
        auto frame = file.read_frame(i);
        logger::info(i, ": frame_number:", file.frame_number(i));
    }

    logger::info("Cluster file");
    logger::info("nclusters:", clusters.size());
    logger::info("frame_number", clusters[0].frame_number);

    // auto clusters = clusterFinder.find_clusters(frame.span(), p);

    // aare::logger::info("nclusters:", clusters.size());

    // for (auto &cluster : clusters) {
    //     aare::logger::info("cluster center:", cluster.to_string<double>());
    // }
}