#include "aare.hpp"
#include "aare/examples/defs.hpp"

using namespace aare;

int main() {
    v3::ClusterFile::Header header;
    header.version = "0.1";
    header.n_records = 100;
    header.metadata["test"] = "test";
    header.header_fields.push_back(
        {"frame_number", Dtype::INT32, v3::ClusterFile::Field::NOT_ARRAY, 1});
    header.header_fields.push_back(
        {"n_clusters", Dtype::INT32, v3::ClusterFile::Field::NOT_ARRAY, 1});
    header.data_fields.push_back({"x", Dtype::INT16, v3::ClusterFile::Field::NOT_ARRAY, 1});
    header.data_fields.push_back({"y", Dtype::INT16, v3::ClusterFile::Field::NOT_ARRAY, 1});
    header.data_fields.push_back(
        {"data", Dtype::INT32, v3::ClusterFile::Field::FIXED_LENGTH_ARRAY, 9});

    v3::ClusterFile file("/tmp/test_cluster.clust2", "w", header);

    // read data file
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv("AARE_ROOT_DIR"));
    std::filesystem::path const fpath(PROJECT_ROOT_DIR / "data" / "clusters" /
                                      "test_cluster.clust2");
    v3::ClusterFile file2(fpath, "r");
    std::pair<v3::ClusterHeader,std::vector<v3::ClusterData>> result = file2.read<v3::ClusterHeader, v3::ClusterData>();
    std::cout<<result.first.to_string()<<std::endl;
    std::cout<<result.second.size()<<'\n';
    for (auto &c : result.second) {
        std::cout << c.to_string() << '\n';
    }
    
}