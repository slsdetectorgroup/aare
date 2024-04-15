#include "aare/file_io/ClusterFile.hpp"
#include "test_config.hpp"
#include <catch2/catch_test_macros.hpp>

using aare::Cluster;
using aare::ClusterFile;
using aare::ClusterFileConfig;

TEST_CASE("Read a cluster file") {
    auto fpath = test_data_path() / "clusters" / "single_frame_97_clustrers.clust";
    REQUIRE(std::filesystem::exists(fpath));
    ClusterFile cf(fpath, "r");

    SECTION("Read the header of the file") {
        REQUIRE(cf.count() == 97);
        REQUIRE(cf.frame() == 135);
    }
    SECTION("Read a single cluster") {
        Cluster c = cf.read();
        REQUIRE(c.x == 1);
        REQUIRE(c.y == 200);
        for (int i = 0; i < 9; i++) {
            REQUIRE(c.data[i] == i);
        }
    }
    SECTION("Read a single cluster using iread") {
        Cluster c = cf.iread(0);
        REQUIRE(c.x == 1);
        REQUIRE(c.y == 200);
        for (int i = 0; i < 9; i++) {
            REQUIRE(c.data[i] == i);
        }
    }
    SECTION("Read a cluster using seek") {
        cf.seek(0);
        Cluster c = cf.read();
        REQUIRE(c.x == 1);
        REQUIRE(c.y == 200);
        for (int i = 0; i < 9; i++) {
            REQUIRE(c.data[i] == i);
        }
        c = cf.read();
        REQUIRE(c.x == 2);
        REQUIRE(c.y == 201);
        for (int i = 0; i < 9; i++) {
            REQUIRE(c.data[i] == i + 9);
        }
    }
    SECTION("check out of bound reading") {
        REQUIRE_THROWS_AS(cf.iread(97), std::runtime_error);
        REQUIRE_NOTHROW(cf.seek(97));
        REQUIRE_THROWS_AS(cf.read(), std::runtime_error);
        REQUIRE_THROWS_AS(cf.read(1), std::runtime_error);
        REQUIRE_NOTHROW(cf.seek(0));
        REQUIRE_NOTHROW(cf.read(97));
    }

    SECTION("test read multiple clusters") {
        std::vector<Cluster> cluster = cf.read(97);
        REQUIRE(cluster.size() == 97);
        int offset = 0;
        int data_offset = 0;
        for (auto c : cluster) {
            REQUIRE(c.x == offset + 1);
            REQUIRE(c.y == offset + 200);
            for (int i = 0; i < 9; i++) {
                REQUIRE(c.data[i] == data_offset + i);
            }

            offset++;
            data_offset += 9;
        }
    }
}