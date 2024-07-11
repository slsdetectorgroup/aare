#include "aare/file_io/ClusterFileV2.hpp"
#include "aare/core/NDArray.hpp"
#include <catch2/catch_test_macros.hpp>

#include "test_config.hpp"

using aare::Dtype;
using aare::ClusterFileV2;
TEST_CASE("Reading a simple cluster file") {

    auto fpath = test_data_path() / "clusters" / "single_frame_97_clustrers.clust";
    REQUIRE(std::filesystem::exists(fpath));

    ClusterFileV2 f(fpath, "r");
    auto clusters = f.read(); // get the first frame
    REQUIRE(clusters.size() == 97);

}

TEST_CASE("Throws when reading a closed file") {
    auto fpath = test_data_path() / "clusters" / "single_frame_97_clustrers.clust";
    REQUIRE(std::filesystem::exists(fpath));

    ClusterFileV2 f(fpath, "r");
    f.close();
    REQUIRE_THROWS(f.read());
}