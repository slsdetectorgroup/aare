#include "aare.hpp"
#include "aare/examples/defs.hpp"

#include <cassert>
#include <iostream>

using namespace aare;
int main() {
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv("AARE_ROOT_DIR"));
    // std::filesystem::path const fpath(PROJECT_ROOT_DIR / "data" / "clusters" / "single_frame_97_clustrers.clust");

    NDArray<double, 2> frame({10, 10});
    frame = 0;

    for (int i = 5; i < 8; i++) {
        for (int j = 5; j < 8; j++) {
            frame(i, j) = (i + j) % 10;
        }
    }

    for (int i = 0; i < frame.shape(0); i++) {
        for (int j = 0; j < frame.shape(1); j++) {
            std::cout << frame(i, j) << " ";
        }
        std::cout << std::endl;
    }

    ClusterFinder clusterFinder(3, 3, 1, 1); // 3x3 cluster, 1 nSigma, 1 threshold

    Pedestal p(10, 10);

    auto clusters = clusterFinder.find_clusters(frame.span(), p);

    aare::logger::info("nclusters:", clusters.size());

    for (auto &cluster : clusters) {
        aare::logger::info("cluster center:", cluster.to_string<double>());
    }
}