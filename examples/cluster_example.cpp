#include "aare.hpp"
#include "aare/examples/defs.hpp"

#include <cassert>
#include <iostream>

using namespace aare;
int main() {
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv("AARE_ROOT_DIR"));
    std::filesystem::path const fpath("/home/bb/tmp/testNewFW20230714/cu_half_speed_master_4.json");
    auto f = File(fpath, "r");
    // calculate pedestal
    Pedestal pedestal(400,400,1000);
    for (int i = 0; i < 1000; i++) {
        auto frame = f.read();
        pedestal.push<uint16_t>(frame);
    }
    // find clusters
    ClusterFinder clusterFinder(3, 3, 5, 0);
    f.seek(0);
    std::vector<std::vector<deprecated::Cluster>> clusters_vector;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        auto frame = f.iread(i);
        auto clusters = clusterFinder.find_clusters_without_threshold(frame.view<uint16_t>(), pedestal,false);
        clusters_vector.emplace_back(clusters);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    std::cout << "elapsed time: " << elapsed_seconds.count() << "s\n";

}