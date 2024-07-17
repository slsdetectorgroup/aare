#include "aare.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <vector>

using namespace aare;
int main() {

    v3::ClusterFileHeader header;
    header.metadata["nSigma"] = "77";

    header.header_fields = v3::ClusterHeader::get_fields();
    header.data_fields = v3::ClusterDataVlen::get_fields();

    v3::ClusterFile<v3::ClusterHeader, v3::ClusterDataVlen> file("/tmp/test_vlen.clust2", "w",
                                                                 header);

    v3::ClusterHeader cluster_header;
    cluster_header.frame_number = 44;
    cluster_header.n_clusters = 10;
    std::vector<v3::ClusterDataVlen> clusters;
    for (auto i = 0; i < 10; i++) {
        v3::ClusterDataVlen cluster;
        cluster.x = std::vector<int16_t>(i+1, i);
        cluster.y = std::vector<int16_t>(i+1, 10 + i);
        cluster.energy = std::vector<int32_t>(i+1, 20.0 + i);
        clusters.emplace_back(cluster);
    }
    std::cout << "Cluster header: " << cluster_header.to_string() << std::endl;
    file.write(cluster_header, clusters);
    file.close();

    v3::ClusterFile<v3::ClusterHeader, v3::ClusterDataVlen> file2("/tmp/test_vlen.clust2", "r");

    auto [header2, clusters2] = file2.read();
    std::cout << "Cluster header: " << header2.to_string() << std::endl;
    for (auto &c : clusters2) {
        std::cout << c.to_string() << std::endl;
    }

    bool exception_thrown = false;
    try{
        file2.read();
    } catch(std::invalid_argument &e) {
        exception_thrown = true;
    }
    assert(exception_thrown);



    return 0;
}