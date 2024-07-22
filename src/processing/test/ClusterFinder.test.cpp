#include "aare/processing/ClusterFinder.hpp"
#include "aare/processing/Pedestal.hpp"
#include "aare/utils/floats.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <random>

using namespace aare;

class ClusterFinderUnitTest : public ClusterFinder {
  public:
    ClusterFinderUnitTest(int cluster_size_x, int cluster_size_y, double nSigma = 5.0,
                          double threshold = 0.0)
        : ClusterFinder(cluster_size_x, cluster_size_y, nSigma, threshold) {}
    double get_c2() { return c2; }
    double get_c3() { return c3; }
    auto get_threshold() { return m_threshold; }
    auto get_nSigma() { return m_nSigma; }
    auto get_cluster_sizeX() { return m_cluster_size_x; }
    auto get_cluster_sizeY() { return m_cluster_size_y; }
};

TEST_CASE("test ClusterFinder constructor") {
    ClusterFinderUnitTest cf(55, 100, 5, 0);
    REQUIRE(cf.get_threshold() == 0.0);
    REQUIRE(cf.get_nSigma() == 5.0);
    double c2 = sqrt((100 + 1) / 2 * (55 + 1) / 2);
    double c3 = sqrt(55 * 100);
    REQUIRE(compare_floats<double>(cf.get_c2(), c2));
    REQUIRE(compare_floats<double>(cf.get_c3(), c3));
}

TEST_CASE("test cluster finder") {
    aare::Pedestal pedestal(10, 10, 5);
    NDArray<double, 2> frame({10, 10});
    frame = 0;
    ClusterFinder clusterFinder(3, 3, 1, 1); // 3x3 cluster, 1 nSigma, 1 threshold

    auto clusters = clusterFinder.find_clusters_without_threshold(frame.span(), pedestal);

    REQUIRE(clusters.size() == 0);

    frame(5, 5) = 10;
    clusters = clusterFinder.find_clusters_without_threshold(frame.span(), pedestal);
    REQUIRE(clusters.size() == 1);
    REQUIRE(clusters[0].x == 5);
    REQUIRE(clusters[0].y == 5);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (i == 1 && j == 1)
                REQUIRE(clusters[0].get_array<double>(i * 3 + j) == 10);
            else
                REQUIRE(clusters[0].get_array<double>(i * 3 + j) == 0);
        }
    }
}