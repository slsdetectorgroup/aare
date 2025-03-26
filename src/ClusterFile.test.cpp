#include "aare/ClusterFile.hpp"
#include "test_config.hpp"


#include "aare/defs.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>




using aare::ClusterFile;

TEST_CASE("Read one frame from a a cluster file", "[.integration]") {
    //We know that the frame has 97 clusters
    auto fpath = test_data_path() / "clusters" / "single_frame_97_clustrers.clust";
    REQUIRE(std::filesystem::exists(fpath));

    ClusterFile f(fpath);
    auto clusters = f.read_frame();
    REQUIRE(clusters.size() == 97);
    REQUIRE(clusters.frame_number() == 135);
}

TEST_CASE("Read one frame using ROI", "[.integration]") {
    //We know that the frame has 97 clusters
    auto fpath = test_data_path() / "clusters" / "single_frame_97_clustrers.clust";
    REQUIRE(std::filesystem::exists(fpath));

    ClusterFile f(fpath);
    aare::ROI roi;
    roi.xmin = 0;
    roi.xmax = 50;
    roi.ymin = 200;
    roi.ymax = 249;
    f.set_roi(roi);
    auto clusters = f.read_frame();
    REQUIRE(clusters.size() == 49);
    REQUIRE(clusters.frame_number() == 135);

    //Check that all clusters are within the ROI
    for (size_t i = 0; i < clusters.size(); i++) {
        auto c = clusters.at<aare::Cluster3x3>(i);
        REQUIRE(c.x >= roi.xmin);
        REQUIRE(c.x <= roi.xmax);
        REQUIRE(c.y >= roi.ymin);
        REQUIRE(c.y <= roi.ymax);
    }

}


TEST_CASE("Read clusters from single frame file", "[.integration]") {

    auto fpath = test_data_path() / "clusters" / "single_frame_97_clustrers.clust";
    REQUIRE(std::filesystem::exists(fpath));

    ClusterFile f(fpath);
    auto clusters = f.read_clusters(500);
    REQUIRE(clusters.size() == 97);


    //Cluster vector should hold the last read frame number:
    REQUIRE(clusters.frame_number() == 135);
}
