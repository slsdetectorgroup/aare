#include "aare.hpp"
using namespace aare;

int main() {
    ClusterFileV3::Header header;
    header.version = "0.1";
    header.n_records = 100;
    header.metadata["test"] = "test";
    header.header_fields.push_back({"frame_number", Dtype::INT32, ClusterFileV3::Field::NOT_ARRAY, 1});
    header.header_fields.push_back({"n_clusters", Dtype::INT32, ClusterFileV3::Field::NOT_ARRAY, 1});
    header.data_fields.push_back({"x", Dtype::INT16, ClusterFileV3::Field::NOT_ARRAY, 1});
    header.data_fields.push_back({"y", Dtype::INT16, ClusterFileV3::Field::NOT_ARRAY, 1});
    header.data_fields.push_back({"data", Dtype::INT32, ClusterFileV3::Field::FIXED_LENGTH_ARRAY, 9});

    ClusterFileV3 file("/tmp/test_cluster.clust2", "w", header);
}