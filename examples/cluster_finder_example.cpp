
#include "aare.hpp"
#include "aare/examples/defs.hpp"

#include <cassert>
#include <iostream>

using namespace aare;
int main() {
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv("AARE_ROOT_DIR"));
    std::filesystem::path const fpath_frame("/home/l_bechir/tmp/testNewFW20230714/cu_half_speed_master_4.json");
    std::filesystem::path const fpath_cluster("/home/l_bechir/tmp/testNewFW20230714/clust/cu_half_speed_d0_f0_4.clust");
    auto file = RawFile(fpath_frame, "r");

    logger::info("RAW file");
    logger::info("file rows:", file.rows(), "cols:", file.cols());
    logger::info(file.total_frames());
    Frame frame(0, 0, 0);
    for (auto i = 0; i < 10000; i++) {
        if (file.frame_number(i) == 23389) {
            logger::info("frame_number:", file.frame_number(i));
            frame = file.read();
        }
    }
    logger::info("frame", frame.rows(), frame.cols(), frame.bitdepth());

    ClusterFileV2 cf(fpath_cluster, "r");
    auto anna_clusters = cf.read();
    logger::info("Cluster file");
    logger::info("nclusters:", anna_clusters.size());
    logger::info("frame_number", anna_clusters[0].frame_number);

    ClusterFinder clusterFinder(3, 3, 5, 0);
    Pedestal p(file.rows(), file.cols());
    file.seek(0);
    logger::info("Starting Pedestal calculation");
    for (auto i = 0; i < 1000; i++) {
        p.push(file.read().view<uint16_t>());
    }
    logger::info("Pedestal calculation done");
    logger::info("Pedestal mean:", p.mean(0, 0), "std:", p.standard_deviation(0, 0));
    logger::info("Pedestal mean:", p.mean(200, 200), "std:", p.standard_deviation(200, 200));
    FileConfig cfg;
    cfg.dtype = DType(typeid(double));
    cfg.rows = p.rows();
    cfg.cols = p.cols();

    NumpyFile np_pedestal("/home/l_bechir/tmp/testNewFW20230714/pedestal.npy", "w", cfg);
    cfg.dtype = DType(typeid(uint16_t));
    NumpyFile np_frame("/home/l_bechir/tmp/testNewFW20230714/frame.npy", "w", cfg);

    np_pedestal.write(p.mean());
    np_frame.write(frame.view<uint16_t>());

    auto clusters = clusterFinder.find_clusters_without_threshold(frame.view<uint16_t>(), p);
    logger::info("nclusters:", clusters.size());

    // aare::logger::info("nclusters:", clusters.size());

    // for (auto &cluster : clusters) {
    //     aare::logger::info("cluster center:", cluster.to_string<double>());
    // }
}