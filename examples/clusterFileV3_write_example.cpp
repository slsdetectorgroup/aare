#include "aare.hpp"
#include "aare/examples/defs.hpp"

using namespace aare;

int main() {
    ClusterFileHeader header;
    header.version = "0.1";
    header.n_records = 100;
    header.metadata["test"] = "test";
    header.header_fields.push_back(
        {"frame_number", Dtype::INT32, Field::NOT_ARRAY, 1});
    header.header_fields.push_back(
        {"n_clusters", Dtype::INT32, Field::NOT_ARRAY, 1});
    header.data_fields.push_back({"x", Dtype::INT16, Field::NOT_ARRAY, 1});
    header.data_fields.push_back({"y", Dtype::INT16, Field::NOT_ARRAY, 1});
    header.data_fields.push_back(
        {"data", Dtype::INT32, Field::FIXED_LENGTH_ARRAY, 9});

    ClusterFile<ClusterHeader,ClusterData<9>> file("/tmp/test_cluster2.clust2", "w", header);
    for (int j = 0; j < 1000; j++) {
        ClusterHeader clust_header;
        std::vector<ClusterData<9>> clust_data;

        clust_header.frame_number = j;
        clust_header.n_clusters = 500 + j;
        for (int i = 0; i < clust_header.n_clusters; i++) {
            ClusterData c;
            c.m_x = i;
            c.m_y = j;
            c.m_data = {1, 2, 3, 4, 5, 6, 7, 8, 9};
            clust_data.push_back(c);
        }
        file.write(clust_header, clust_data);
    }
    file.close();

    // auto PROJECT_ROOT_DIR = std::filesystem::path(getenv("AARE_ROOT_DIR"));
    // std::filesystem::path const fpath(PROJECT_ROOT_DIR / "data" / "clusters" /
    //                                   "test_cluster.clust2");
    // ClusterFile file2(fpath, "r");
    // std::pair<ClusterHeader, std::vector<ClusterData>> result =
    //     file2.read<ClusterHeader, ClusterData>();

    // // read data file
    // auto PROJECT_ROOT_DIR = std::filesystem::path(getenv("AARE_ROOT_DIR"));
    // std::filesystem::path const fpath(PROJECT_ROOT_DIR / "data" / "clusters" /
    //                                   "test_cluster.clust2");
    // ClusterFile file2(fpath, "r");
    // std::pair<ClusterHeader, std::vector<ClusterData>> result =
    //     file2.read<ClusterHeader, ClusterData>();
    // std::cout << result.first.to_string() << std::endl;
    // std::cout << result.second.size() << '\n';
    // for (auto &c : result.second) {
    //     std::cout << c.to_string() << '\n';
    // }
}