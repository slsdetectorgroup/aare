#include <catch2/catch_test_macros.hpp>
#include "test_config.hpp"
#include "aare/file_io/ClusterFile.hpp"

using aare::ClusterFile;
using aare::Cluster;
using aare::ClusterFileConfig;

TEST_CASE("Read a cluster file") {
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv("AARE_ROOT_DIR"));
    std::filesystem::path const fpath(PROJECT_ROOT_DIR / "data" / "clusters" / "single_frame_97_clustrers.clust");

    ClusterFile cf(fpath, "r");
    REQUIRE(cf.count() == 97);
    REQUIRE(cf.frame() == 0);
    cf.seek(0); // seek to the beginning of the file (this is the default behavior of the constructor)

    auto cluster = cf.read(97);

    int offset = 0;
    int data_offset = 0;
    for (auto c : cluster) {
        REQUIRE(c.x == offset);
        REQUIRE(c.y == offset + 200);
        for (int i = 0; i < 9; i++) {
            REQUIRE(c.data[i] == data_offset + i);
        }

        offset++;
        data_offset += 9;
    }
}